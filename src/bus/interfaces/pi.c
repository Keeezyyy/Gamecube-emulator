#include "pi.h"
#include <_abort.h>
#include <assert.h>

u32 pi_read(u32 adr)
{
    if (adr == 0xcc00302c) {
        return 0x20000000;
    } else {
        assert(!"read pi not implemented\n");
    }
}

u32 pi_write(CPU *cpu, u32 adr, u32 val)
{
    if (adr == 0xCC003004) {
        // set interrupt mask

        cpu->exception.interrupt_mask_register = val;
    } else {

        assert(!"write pi not implemented\n");
    }
}
