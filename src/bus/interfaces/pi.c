#include "pi.h"

u32 pi_read(u32 adr)
{
    if (adr == 0xcc00302c) {
        return 0x20000000;
    }
}
