#include "cpu/cpu.h"
#include <pthread.h>
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/translation.h"
#include "disc/disc.h"
#include <_abort.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_pthread/_pthread_t.h>
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

static void main_loop(CPU *self)
{
    TranslationBlock *tb;
    CpuMode m = self->get_current_cpu_mode(self);

    // TODO: implement interrupt check and isr
    // while (!waiting_interrupt(self)) {
    while (true) {
        // DEBUG_PRINT("cycle \n");

        tb = tb_lookup(self, m);
        if (tb == NULL_PTR) {
            // TODO:
            // code block is not present in hash table and has to be translated

            // NOTE: if I add thread make sure to use locks here

            TranslationBlock *new_tb = malloc(sizeof(TranslationBlock));

            assert(new_tb != NULL_PTR);

            if (!tb_translate(self, m, new_tb)) {
                fprintf(stderr, "translation failed at pc 0x%08x\n", self->state.pc);
                free(new_tb);
                abort();
            }

            if (tb_finilize(new_tb) != 0) {
                fprintf(stderr, "tb_finilize failed at pc 0x%08x\n", self->state.pc);
                free(new_tb);
                abort();
            }

            tb = new_tb;
        }

        self->print_state(self);
        run_tb(tb, self);
        self->print_state(self);
    }
}

static void _boot(CPU *self)
{
    self->state.msr = 0;

    self->state.msr = BIT_SET(self->state.msr, 25); // boot bit

    self->state.pc = 0xfff00100;
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

void _helper_write_word_to_bus(u32 adr, u32 val)
{
    // DEBUG_PRINT("[WRITE] : writing 0x%08x , to : 0x%08x\n", val, adr);
    static_cpu_ptr->bus->write(static_cpu_ptr->bus, adr, val);
}
static void _helper_write_byte_to_bus(u32 adr, u32 val)
{
    // DEBUG_PRINT("[WRITE] : writing 0x%08x , to : 0x%08x\n", val, adr);
    static_cpu_ptr->bus->write_byte(static_cpu_ptr->bus, adr, val);
}
static void _helper_write_half_to_bus(u32 adr, u32 val)
{
    static_cpu_ptr->bus->write_half(static_cpu_ptr->bus, adr, val);
}
static u32 _helper_read_word_from_bus(u32 adr)
{
    u32 val = static_cpu_ptr->bus->read_word(static_cpu_ptr->bus, adr);

    // DEBUG_PRINT("[READ] : reading 0x%08x , from : 0x%08x\n", val, adr);
    return val;
}
static u64 _helper_read_double_word_from_bus(u32 adr)
{
    u64 val = static_cpu_ptr->bus->read_dword(static_cpu_ptr->bus, adr);

    // DEBUG_PRINT("[READ] : reading 0x%016llx , from : 0x%08x\n", val, adr);
    return val;
}
static u32 _helper_read_byte(u32 adr)
{
    const u8 val = static_cpu_ptr->bus->read_byte(static_cpu_ptr->bus, adr);

    // DEBUG_PRINT("[READ] : reading 0x%016llx , from : 0x%08x\n", val, adr);
    return val & 0xff;
}

static u8 _fpu_get_sep_bit(CPU *self)
{
    return (self->special_purpose_registers.hid2 >> 31 - 2) & 1;
}

static const FPU FPU_TEMPLATE = {
    .get_pse_bit = _fpu_get_sep_bit,
};

static void start(CPU *self)
{
    pthread_t main_thread;
    pthread_t background_thread;

    // NOTE: pthread_create writes the new handle through the first argument, so it must be the
    // address of the pthread_t, not its (uninitialized) value
    if (pthread_create(&main_thread, NULL, (void *)self->main, self) != 0) {
        abort();
    }
    if (pthread_create(&background_thread, NULL, (void *)self->background, self) != 0) {
        abort();
    }

    pthread_join(main_thread, NULL);
    pthread_join(background_thread, NULL);
}

static const CPU CPU_TEMPLATE = {
    .registers = {0},
    .state = {0},
    .free = &deconstruct_cpu,
    .main = &main_loop,
    .start = &start,
    .background = &run_background,
    .get_current_cpu_mode = &get_current_cpu_mode,
    .boot = &_boot,
    .print_state = &print_cpu_state,
    .fpu = FPU_TEMPLATE,

    // NOTE: no libc functions !!!
    // NOTE if usage of lib functions in debug push and pop float regs
    //------------------------------------------------------------------------------------------
    .helper_functions[0] = (u64)&_helper_write_word_to_bus,
    .helper_functions[1] = (u64)&_helper_read_word_from_bus,
    .helper_functions[2] = (u64)&_helper_read_double_word_from_bus,
    .helper_functions[3] = (u64)&_helper_read_byte,
    .helper_functions[4] = (u64)&_helper_write_byte_to_bus,
    .helper_functions[5] = (u64)&_helper_write_half_to_bus,
    //------------------------------------------------------------------------------------------
};

void init_cpu(CPU *self, Disc *disc, Bus *bus)
{
    *self = CPU_TEMPLATE;
    self->bus = bus;
    init_translation(disc);
    static_cpu_ptr = self;
}
