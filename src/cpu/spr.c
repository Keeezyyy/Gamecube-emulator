#include "spr.h"
#include "bus/bus.h"
#include "core/config/config.h"

static u64 tb_offset = 0;

static u64 _tb_now(CPU *cpu)
{
    return tb_offset + cpu->cpu_cycles / 12;
}

u32 time_base_read(CPU *cpu, const u32 adr)
{
    const u64 tb = _tb_now(cpu);
    return adr == SPR_TIME_BASE_L ? (u32)tb : (u32)(tb >> 32);
}

void time_base_write(CPU *cpu, const u32 adr, const u32 val)
{
    u64 tb = _tb_now(cpu);
    if (adr == SPR_TIME_BASE_L)
        tb = (tb & (u64)(U32_MAX) << 32) | val;
    else
        tb = ((u64)val << 32) | (u32)tb;
    tb_offset = tb - cpu->cpu_cycles / 12;
}
