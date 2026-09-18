#include "interface_utils.h"
#include "core/config/config.h"

void set_register(u32 *r, u32 val, int *w1cs, size_t w1cs_length)
{
    int mask = 0;
    for (int i = 0; i < w1cs_length; i++) {
        int w1c = w1cs[i];
        mask |= 1 << w1c;
        if (((val >> w1c) & 1) == 0)
            continue;
        if (((*r >> w1c) & 1) == 1)
            BIT_CLEAR(*r, w1c);
        BIT_SET(*r, w1c);
    }
    *r &= ~mask;
    *r |= val & mask;
}
