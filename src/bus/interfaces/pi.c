#include "pi.h"
#include <_abort.h>
#include <assert.h>

u64 pi_read(CPU *cpu, u32 adr, u32 size)
{
    if (adr == 0xcc00302c) {
        return 0x20000000;
    }

    assert(!"read pi not implemented\n");
    return 0;
}

void pi_write(CPU *cpu, u32 adr, u64 val, u32 size)
{
    if (adr == 0xCC003004) {
        // set interrupt mask

        cpu->exception.interrupt_mask_register = val;
    } else {

        assert(!"write pi not implemented\n");
    }
}
