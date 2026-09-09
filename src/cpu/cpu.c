#include "cpu/cpu.h"

#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/translation.h"
#include "disc/disc.h"
#include <stdbool.h>

static void deconstruct_cpu(CPU *self)
{
    UNUSED(self);
    deconstruct_translation();
}

static CpuMode get_current_cpu_mode(CPU *self)
{
    // TODO: implement
    CpuMode m;
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
            //
        }
    }
}

static const CPU CPU_TEMPLATE = {
    .registers = {0},
    .state = {0},
    .free = &deconstruct_cpu,
    .main = &main_loop,
    .get_current_cpu_mode = &get_current_cpu_mode,
};

void init_cpu(CPU *self, Disc *disc)
{
    *self = CPU_TEMPLATE;
    init_translation(disc);
}
