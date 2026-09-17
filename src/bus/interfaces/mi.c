
#include "mi.h"

#include "cpu/cpu_types.h"
#include <assert.h>

static u64 mi_interrupt_mask; // upper 48 bits are ignored

void mi_write(CPU *cpu, u32 adr, u64 val, u32 size)
{
    if (adr == 0xCC00401C) {
        // set interrupt mask

        mi_interrupt_mask = val;
    } else {

        assert(!"write pi not implemented\n");
    }
}
