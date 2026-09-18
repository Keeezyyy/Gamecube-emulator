#include "vi.h"
#include <assert.h>

static DisplayRegisters dpr;
void vi_write(CPU *cpu, u32 adr, u32 val, u32 size)
{
    assert(size == 2 || size == 4);
    if (size == 2) {
        u16 v = val;
        ((u16 *)&dpr)[adr - 0xCC002000] = v;
    } else {
        ((u32 *)&dpr)[adr - 0xCC002000] = val;
    }
}

u32 vi_read(CPU *cpu, u32 adr, u32 size)
{
    if (adr == 0xCC002002) {
        return dpr.DCR & ~(2);
    }
}
