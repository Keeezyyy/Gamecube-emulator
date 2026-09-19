#include "pi.h"
#include "core/config/config.h"
#include <_abort.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static PIRegs pi_regs;
u64 pi_read(CPU *cpu, u32 adr, u32 size)
{
    if (adr == 0xcc00302c) {
        return 0x20000000;
    }
    if (adr == 0xcc003024) {
        return 0x0;
    }

    assert(!"read pi not implemented\n");
    return 0;
}

static u16 processor_interface_control_register = 0;

void pi_write(CPU *cpu, u32 adr, u64 val, u32 size)
{
    switch (adr) {
    case 0xCC003004: {
        cpu->exception.interrupt_mask_register = val;
        break;
    }
    case 0xCC00300c: {
        pi_regs.PI_FIFO_BASE = val & 0xFFFFFFE0;
        break;
    }
    case 0xCC003010: {
        pi_regs.PI_FIFO_END = val & 0xFFFFFFE0;
        break;
    }
    case 0xCC003014: {
        pi_regs.PI_FIFO_WPTR = val;
        break;
    }
    default:
        assert(!"write pi not implemented\n");
    }
}

void pi_recieve_gx_gather_piper(CPU *cpu, u32 *buffer)
{
    printf("gather pipe wptr : 0x%08x\n", pi_regs.PI_FIFO_WPTR);
    memcpy(&((u8 *)cpu->bus->ram)[pi_regs.PI_FIFO_WPTR], buffer, 32);

    if (pi_regs.PI_FIFO_WPTR == pi_regs.PI_FIFO_END) {
        pi_regs.PI_FIFO_WPTR = pi_regs.PI_FIFO_BASE;
    }
}

void pi_activate_external_interrupt(CPU *cpu, u8 interrupt_source)

{
    cpu->exception.interrupt_source_register |= BIT(interrupt_source);
}

void pi_deactivate_external_interrupt(CPU *cpu, u8 interrupt_source)
{
    cpu->exception.interrupt_source_register &= ~BIT(interrupt_source);
}
