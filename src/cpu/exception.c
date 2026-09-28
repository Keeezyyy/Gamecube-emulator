#include "cpu.h"
#include "cpu/translation/translation.h"
#include <assert.h>
#include <stdlib.h>

static u32 handler_adresses[0x18] = {
    [INTERNAL_INTERRUPT_TYPE_SYSTEM_RESET] = 0x80000100,
    [INTERNAL_INTERRUPT_TYPE_MACHINE_CHECK] = 0x80000200,
    [INTERNAL_INTERRUPT_TYPE_DSI] = 0x80000300,
    [INTERNAL_INTERRUPT_TYPE_ISI] = 0x80000400,
    [INTERNAL_INTERRUPT_TYPE_EXTERNAL] = 0x80000500,
    [INTERNAL_INTERRUPT_TYPE_ALIGNMENT] = 0x80000600,
    [INTERNAL_INTERRUPT_TYPE_PROGRAM] = 0x80000700,
    [INTERNAL_INTERRUPT_TYPE_FP_UNAVAILABLE] = 0x80000800,
    [INTERNAL_INTERRUPT_TYPE_DECREMENT] = 0x80000900,
    [INTERNAL_INTERRUPT_TYPE_SYSTEM_CALL] = 0x80000C00,
    [INTERNAL_INTERRUPT_TYPE_TRACE] = 0x80000D00,
    [INTERNAL_INTERRUPT_TYPE_PERFORMANCE_MONITOR] = 0x80000F00,
    [INTERNAL_INTERRUPT_TYPE_IABR] = 0x80001300,
    [INTERNAL_INTERRUPT_TYPE_THERMAL] = 0x80001700,
};

void handle_interrupt(CPU *self, enum CpuInternalInterruptType type)
{
    TranslationBlock *tb;
    CpuMode m = self->get_current_cpu_mode(self);

    _helper_write_switch_to_exception(self->state.pc - 4);

    self->state.pc = handler_adresses[type];
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

    self->trigger_internal_interrupt(self, type, false);
}
