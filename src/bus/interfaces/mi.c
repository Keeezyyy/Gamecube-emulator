
#include "mi.h"

#include "bus/interfaces/interface_utils.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include <assert.h>
#include <stdio.h>

static MemoryInterfaceRegisters mi_regs;

void mi_write(CPU *cpu, u32 adr, u64 val, u32 size)
{

    printf("MI : adr : 0x%08x, val : 0x%08x\n", adr, val);

    assert(size == 2);

    if (adr == 0xCC00401E) {
        const int w1cs[] = {0, 1, 2, 3, 4};
        set_register(&mi_regs.irqflag, (u32)val, w1cs, ARRAY_SIZE(w1cs));
        return;
    }

    const u32 rel_adr = adr - 0xCC004000;

    const u8 *mem_ptr = &mi_regs;
    *(u16 *)(&mem_ptr[rel_adr]) = (u16)val;
}

u32 mi_read(CPU *cpu, u32 adr, u32 size)
{

    printf("MI read : adr : 0x%08x\n", adr);

    return 0;
}

void mi_report_mem_access(CPU *cpu, u32 adr, MiMemAccessType access_type)
{
}
