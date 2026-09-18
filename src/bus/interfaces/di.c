#include "di.h"
#include "bus/interfaces/interface_utils.h"
#include "core/config/config.h"
#include <arm/types.h>
#include <assert.h>
#include <stdio.h>

u64 di_read(CPU *cpu, u32 adr, u32 size)
{
    if (adr == 0xCC006024) {
        // bootrom scrambler disabled = 1
        return 1;
    }
}

static u32 disc_register;
static u32 disc_cover_register;
void di_write(CPU *cpu, u32 adr, u32 val, u32 size)
{
    assert(size == 4);
    switch (adr) {
    case 0xCC006000: {
        int w1cs[] = {2, 4, 6};
        set_register(&disc_register, val, w1cs, ARRAY_SIZE(w1cs));
        return;
    }
    case 0xCC006004: {
        // DISC COVER REGISTER
        int w1cs[] = {2};
        set_register(&disc_cover_register, val, w1cs, ARRAY_SIZE(w1cs));
        return;
    }
    }

    assert(!"dvd not implemented");
}
