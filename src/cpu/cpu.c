#include "cpu/cpu.h"

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
        // printf("cycle \n");

        tb = tb_lookup(self, m);
        if (tb == NULL_PTR) {
            // TODO:
            // code block is not present in hash table and has to be translated

            // NOTE: if I add thread make sure to use locks here

            TranslationBlock *new_tb = malloc(sizeof(TranslationBlock));

            assert(new_tb != NULL_PTR);

            tb_translate(self, m, new_tb);

            assert(tb_finilize(new_tb) == 0);

            self->print_state(self);
            run_tb(new_tb);

            self->print_state(self);
        }
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

    printf("================== CPU ==================\n");
    printf("PC    = 0x%08X\n", st->pc);
    printf("CR    = 0x%08X\n", st->cr);
    printf("XER   = 0x%08X\n", st->xer);
    printf("FPSCR = 0x%08X\n", st->fpscr);
    printf("MSR   = 0x%08X\n", st->msr);

    printf("\n------------------ GPR ------------------\n");
    for (u32 i = 0; i < 32; i++)
        printf("r%-2u = 0x%08X%s", i, rg->gpio[i], (i % 4 == 3) ? "\n" : "  ");

    printf("\n------------------ SR -------------------\n");
    for (u32 i = 0; i < 16; i++)
        printf("sr%-2u = 0x%08X%s", i, rg->sr[i], (i % 4 == 3) ? "\n" : "  ");

    printf("\n------------------ FPR ------------------\n");
    for (u32 i = 0; i < 32; i++) {
        u32 bits;
        memcpy(&bits, &rg->fpr[i], sizeof bits);
        printf("f%-2u = %13.6g (0x%08X)%s", i, (double)rg->fpr[i], bits,
               (i % 2 == 1) ? "\n" : "  ");
    }

    printf("\n------------------ SPR ------------------\n");
    printf("LR   = 0x%08X   (spr8)\n", spr->lr);
    printf("HID2 = 0x%08X   (spr920)\n", spr->hid2);
    printf("IABR = 0x%08X   (spr1010)\n", spr->iabr);
    printf("DABR = 0x%08X   (spr1013)\n", spr->dabr);
    printf("=========================================\n");
}

void _helper_write_word_to_bus(u32 adr, u32 val)
{
    printf("[WRITE] : writing 0x%08x , to : 0x%08x\n", val, adr);
    static_cpu_ptr->bus->write(static_cpu_ptr->bus, adr, val);
}
u32 _helper_read_word_from_bus(u32 adr)
{
    u32 val = static_cpu_ptr->bus->read_word(static_cpu_ptr->bus, adr);

    printf("[READ] : reading 0x%08x , from : 0x%08x\n", val, adr);
    return val;
}

static const CPU CPU_TEMPLATE = {
    .registers = {0},
    .state = {0},
    .free = &deconstruct_cpu,
    .main = &main_loop,
    .get_current_cpu_mode = &get_current_cpu_mode,
    .boot = &_boot,
    .print_state = &print_cpu_state,
    .helper_functions[0] = (u64)&_helper_write_word_to_bus,
    .helper_functions[1] = (u64)&_helper_read_word_from_bus,
};

void init_cpu(CPU *self, Disc *disc, Bus *bus)
{
    *self = CPU_TEMPLATE;
    self->bus = bus;
    init_translation(disc);
    static_cpu_ptr = self;
}
