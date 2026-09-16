
#include "mi.h"

#include "cpu/cpu_types.h"
#include <assert.h>

static u64 mi_interrupt_mask; // upper 48 bits are ignored
u64 *mi_write(CPU *cpu, u32 adr)
{
    if (adr == 0xCC00401C) {
        // set interrupt mask

        return &mi_interrupt_mask;
    } else {

        assert(!"write pi not implemented\n");
    }
}
