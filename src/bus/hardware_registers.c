#include "hardware_registers.h"
#include <assert.h>

static GXFifoRegs cp_regs;
void cp_write(CPU *cpu, u32 adr, u32 val, u32 size)

{
    assert(size == 4 || size == 2);
    if (size == 4) {
        ((u32 *)&cp_regs)[adr - 0xCC000000] = val;
    } else {
        ((u16 *)&cp_regs)[adr - 0xCC000000] = (u16)val;
    }
}
u64 cp_read(CPU *cpu, u32 adr, u32 size)
{
    assert(size == 4 || size == 2);
    if (size == 4) {
        return ((u32 *)&cp_regs)[adr - 0xCC000000];

    } else {
        return ((u16 *)&cp_regs)[adr - 0xCC000000];
    }
}
