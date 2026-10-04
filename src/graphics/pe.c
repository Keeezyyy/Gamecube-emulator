#include "pe.h"
#include "bus/bus.h"
#include "bus/interfaces/interface_utils.h"
#include "bus/interfaces/pi.h"
#include "core/config/config.h"
#include <assert.h>
#include <stdio.h>

static PERegs pe_regs;
static u32 token;

u16 pe_get_ctrl(void)
{
    return pe_regs.CTRL;
}

void pe_write(CPU *cpu, u32 adr, u32 val, u32 size)
{
    assert(size == 2);

    if (adr == 0xCC00100A) {
        u32 ctrl = pe_regs.CTRL;
        int w1cs[] = {2, 3};
        set_register(&ctrl, val, w1cs, ARRAY_SIZE(w1cs));
        pe_regs.CTRL = (u16)ctrl;
        pi_update_interrupts(cpu);
        return;
    }

    printf("adr : 0x%08x\n", adr - 0xCC001000);

    // assert(!"pe write");

    ((u16 *)&pe_regs)[(adr - 0xCC001000) >> 1] = (u16)val;
}
u64 pe_read(CPU *cpu, u32 adr, u32 size)
{
    assert(size == 2);

    if (adr >= 0xCC00100E && adr <= 0xCC00102E) {
        switch (adr) {
        case 0xCC00100E:
            return token;
        default:
            assert(!"pe read not implemented\n");
        }
    }

    return ((u16 *)&pe_regs)[(adr - 0xCC001000) >> 1];
}

void pe_set_interrupt(CPU *cpu)
{
    pe_regs.CTRL |= BIT(3);
    pi_update_interrupts(cpu);
}

void pe_set_token(u32 t, CPU *cpu, bool set_interrupt)
{
    token = t;
    if (set_interrupt) {
        pe_regs.CTRL |= BIT(2);
        pi_update_interrupts(cpu);
    }
}
