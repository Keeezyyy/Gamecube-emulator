#include "interface_utils.h"
#include "core/config/config.h"

// Bits in w1cs: 1 schreiben = loeschen. Alle anderen Bits werden normal geschrieben.
void set_register(u32 *r, u32 val, int *w1cs, size_t w1cs_length)
{
    u32 mask = 0;
    for (size_t i = 0; i < w1cs_length; i++)
        mask |= 1u << w1cs[i];
    *r = (*r & ~val & mask) | (val & ~mask);
}

void set_register_read_only(u32 *r, u32 val, int *w1cs, size_t w1cs_length, u32 read_only)
{
    u32 old = *r;
    set_register(r, val, w1cs, w1cs_length);
    *r = (*r & ~read_only) | (old & read_only);
}
