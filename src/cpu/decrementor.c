#include "cpu.h"
#include "cpu/cpu_types.h"

static u32 remainder = 0;

void update_decrementor(CPU *cpu, u64 current, u64 next)
{
    const u32 at_start = cpu->special_purpose_registers.buf[22];

    const u32 diff = next - current + remainder;

    remainder = diff % 12;

    const u32 new = at_start - (diff / 12);

    if ((at_start >> 31) == 0 && (new >> 31) == 1) {
        cpu->trigger_internal_interrupt(cpu, INTERNAL_INTERRUPT_TYPE_DECREMENT, true);
    }
    cpu->special_purpose_registers.buf[22] = new;
}
