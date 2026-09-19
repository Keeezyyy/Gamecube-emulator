#include "cpu/cpu.h"
#include <alloca.h>
#include <pthread.h>
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/translation.h"
#include "disc/disc.h"
#include "scheduler/scheduler.h"
#include <_abort.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_pthread/_pthread_t.h>
#include <sys/cdefs.h>
static CPU *static_cpu_ptr;

static void deconstruct_cpu(CPU *self)
{
    UNUSED(self);
    deconstruct_translation();
}

static CpuMode get_current_cpu_mode(CPU *self)
{
    // TODO: implement
    CpuMode m;
    m.val = self->state.msr;
    return m;
}
#define HLE_OSREPORT 0x8135d924u
#define HLE_OSPANIC 0x8135d9a4u

static void print_guest_string(CPU *self, u32 adr)
{
    for (u32 i = 0; i < 256; i++) {
        char c = (char)self->bus->read(self->bus, adr + i, 1);
        if (c == 0)
            break;
        putchar(c);
    }
}

static void hle_check_panic(CPU *self)
{
    if (self->state.pc != HLE_OSPANIC)
        return;

    printf("[OSPanic] ");
    print_guest_string(self, self->registers.gpio[3]);
    printf(":%u: ", self->registers.gpio[4]);
    print_guest_string(self, self->registers.gpio[5]);
    printf("\n  (LR = 0x%08x)\n", self->special_purpose_registers.lr);
    fflush(stdout);
    assert(!"guest OSPanic");
}

static void handle_interrupt(CPU *self)
{
    TranslationBlock *tb;
    CpuMode m = self->get_current_cpu_mode(self);

    _helper_write_switch_to_exception(self->state.pc - 4);
    DEBUG_PRINT("[INTERRUPT]\n");

    // NOTE:FOR NOW
    self->state.pc = 0x00000500;
    do {

        tb = tb_lookup(self, m);
        if (tb == NULL_PTR) {
            // TODO:
            // code block is not present in hash table and has to be translated

            // NOTE: if I add thread make sure to use locks here

            TranslationBlock *new_tb = calloc(1, sizeof(TranslationBlock));

            assert(new_tb != NULL_PTR);

            if (!tb_translate(self, m, new_tb, false)) {
                fprintf(stderr, "translation failed at pc 0x%08x\n", self->state.pc);
                free(new_tb);
                assert(!"tb_translate failed: guest code block could not be translated");
            }

            if (tb_finilize(new_tb) != 0) {
                fprintf(stderr, "tb_finilize failed at pc 0x%08x\n", self->state.pc);
                free(new_tb);
                assert(!"tb_finilize failed: translated block could not be finalized");
            }

            tb = new_tb;
        }

        self->print_state(self);
        run_tb(tb, self);
        self->print_state(self);
    } while ((tb->type & TRANSLATION_BLOCK_TYPE_RETURN_FROM_INTERRUPT) != 0);
}

static void main_loop(CPU *self)
{
    TranslationBlock *tb;
    CpuMode m = self->get_current_cpu_mode(self);

    // TODO: implement interrupt check and isr
    // while (!waiting_interrupt(self)) {

    while (true) {

        if (self->awaiting_interrupt(self)) {
            handle_interrupt(self);
        }

        hle_check_panic(self);

        tb = tb_lookup(self, m);
        if (tb == NULL_PTR) {
            TranslationBlock *new_tb = calloc(1, sizeof(TranslationBlock));

            assert(new_tb != NULL_PTR);

            if (!tb_translate(self, m, new_tb, false)) {
                fprintf(stderr, "translation failed at pc 0x%08x\n", self->state.pc);
                free(new_tb);
                assert(!"tb_translate failed: guest code block could not be translated");
            }

            if (tb_finilize(new_tb) != 0) {
                fprintf(stderr, "tb_finilize failed at pc 0x%08x\n", self->state.pc);
                free(new_tb);
                assert(!"tb_finilize failed: translated block could not be finalized");
            }

            tb = new_tb;
        }

        report_cycle_count(self, tb->guest_instructions_count);

        run_tb(tb, self);
    }
}

static void _boot(CPU *self)
{
    self->state.msr = 0;
    // self->state.msr = BIT_SET(self->state.msr, 25);

    self->state.pc = self->bus->entry_point;
}
static void print_cpu_state(CPU *self)
{
    const CpuState *st = &self->state;
    const CpuRegisters *rg = &self->registers;
    const CpuSpecialPurposeRegisters *spr = &self->special_purpose_registers;

    DEBUG_PRINT("================== CPU ==================\n");
    DEBUG_PRINT("PC    = 0x%08X\n", st->pc);
    DEBUG_PRINT("CR    = 0x%08X\n", st->cr);
    DEBUG_PRINT("XER   = 0x%08X\n", st->xer);
    DEBUG_PRINT("FPSCR = 0x%08X\n", st->fpscr);
    DEBUG_PRINT("MSR   = 0x%08X\n", st->msr);

    DEBUG_PRINT("\n------------------ GPR ------------------\n");
    for (u32 i = 0; i < 32; i++)
        DEBUG_PRINT("r%-2u = 0x%08X%s", i, rg->gpio[i], (i % 4 == 3) ? "\n" : "  ");

    DEBUG_PRINT("\n------------------ SR -------------------\n");
    for (u32 i = 0; i < 16; i++)
        DEBUG_PRINT("sr%-2u = 0x%08X%s", i, rg->sr[i], (i % 4 == 3) ? "\n" : "  ");

    DEBUG_PRINT("\n------------------ SPR ------------------\n");
    DEBUG_PRINT("LR   = 0x%08X   (spr8)\n", spr->lr);
    DEBUG_PRINT("HID2 = 0x%08X   (spr920)\n", spr->hid2);
    DEBUG_PRINT("IABR = 0x%08X   (spr1010)\n", spr->iabr);
    DEBUG_PRINT("DABR = 0x%08X   (spr1013)\n", spr->dabr);
    DEBUG_PRINT("=========================================\n");
}

static u8 _fpu_get_sep_bit(CPU *self)
{
    return (self->special_purpose_registers.hid2 >> 31 - 2) & 1;
}

static const FPU FPU_TEMPLATE = {
    .get_pse_bit = _fpu_get_sep_bit,
};

static bool _is_interrupt_awaiting(CPU *self)
{
    if ((self->state.msr & 0x8000) == 0) // interrupt enable
        return false;
    return (self->exception.interrupt_source_register & self->exception.interrupt_mask_register) !=
           0;
}

static const CPU CPU_TEMPLATE = {
    .registers = {0},
    .state = {0},
    .free = &deconstruct_cpu,
    .main = &main_loop,
    .get_current_cpu_mode = &get_current_cpu_mode,
    .boot = &_boot,
    .print_state = &print_cpu_state,
    .fpu = FPU_TEMPLATE,
    .awaiting_interrupt = &_is_interrupt_awaiting,
    // NOTE: no libc functions !!!
    // NOTE if usage of lib functions in debug push and pop float regs
    //------------------------------------------------------------------------------------------
};

void init_cpu(CPU *self, Disc *disc, Bus *bus)
{
    *self = CPU_TEMPLATE;
    self->bus = bus;
    init_translation(disc);
    static_cpu_ptr = self;
    set_cpu_helper(self);
}
