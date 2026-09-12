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

        tb = tb_lookup(self, m);
        if (tb == NULL_PTR) {
            // TODO:
            // code block is not present in hash table and has to be translated

            // NOTE: if I add thread make sure to use locks here
            TranslationBlock new_tb = tb_translate(self, m);

            assert(tb_finilize(&new_tb) == 0);

            run_tb(new_tb.core.code);

            abort();
        }
    }
}

static void _boot(CPU *self)
{
    self->state.msr = 0;

    self->state.msr = BIT_SET(self->state.msr, 25); // boot bit

    self->state.pc = 0xfff00100;
}

static const CPU CPU_TEMPLATE = {
    .registers = {0},
    .state = {0},
    .free = &deconstruct_cpu,
    .main = &main_loop,
    .get_current_cpu_mode = &get_current_cpu_mode,
    .boot = &_boot,
};

void init_cpu(CPU *self, Disc *disc, Bus *bus)
{
    *self = CPU_TEMPLATE;
    self->bus = bus;
    init_translation(disc);
}
