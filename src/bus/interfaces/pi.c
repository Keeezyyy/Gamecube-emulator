#include "pi.h"
#include <_abort.h>
#include <assert.h>

static u64 read_buffer = 0;

u64 *pi_read(u32 adr)
{
    if (adr == 0xcc00302c) {
        read_buffer = 0x20000000;
        return &read_buffer;
    } else {
        assert(!"read pi not implemented\n");
    }
}

u64 *pi_write(CPU *cpu, u32 adr)
{
    if (adr == 0xCC003004) {
        // set interrupt mask

        &cpu->exception.interrupt_mask_register;
    } else {

        assert(!"write pi not implemented\n");
    }
}
