#include "pe.h"
#include <assert.h>
#include <stdio.h>

static PERegs pe_regs;

void pe_write(CPU *cpu, u32 adr, u32 val, u32 size)
{
    assert(size == 2);

    if (adr == 0xCC00100A) {
        pe_regs.CTRL &= ~(val & 12);

        pe_regs.CTRL |= val & 3;
        return;
    }

    printf("adr : 0x%08x\n", adr - 0xCC001000);

    ((u16 *)&pe_regs)[adr - 0xCC001000] = (u16)val;
}
u64 pe_read(CPU *cpu, u32 adr, u32 size)
{
    assert(size == 2);

    return ((u16 *)&pe_regs)[adr - 0xCC001000];
}
