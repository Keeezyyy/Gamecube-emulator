#include "di.h"

u64 di_read(CPU *cpu, u32 adr, u32 size)
{
    if (adr == 0xCC006024) {
        // bootrom scrambler disabled = 1
        return 1;
    }
}
