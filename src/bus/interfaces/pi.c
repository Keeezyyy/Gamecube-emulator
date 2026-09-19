#include "pi.h"
#include "bus/bus.h"
#include "bus/interfaces/ai.h"
#include "bus/interfaces/di.h"
#include "bus/interfaces/exi.h"
#include "bus/interfaces/si.h"
#include "core/config/config.h"
#include "graphics/pe.h"
#include "graphics/vi.h"
#include <_abort.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static PIRegs pi_regs;
u64 pi_read(CPU *cpu, u32 adr, u32 size)
{
    if (adr == 0xcc003000) {
        return cpu->exception.interrupt_source_register;
    }
    if (adr == 0xcc003004) {
        return cpu->exception.interrupt_mask_register;
    }
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
    case 0xCC003000: {
        cpu->exception.interrupt_source_register &= ~val;
        break;
    }
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

#define DI_DISR_INTERRUPT_MASK 0x0000002A
#define DI_DICVR_INTERRUPT_MASK 0x00000002
#define SI_COMCSR_INTERRUPT_MASK 0x48000000
#define EXI_CSR_INTERRUPT_MASK 0x00000405
#define AI_AICR_INTERRUPT_MASK 0x00000004
#define DSP_CSR_INTERRUPT_MASK 0x000000A8
#define VI_DI_INTERRUPT_MASK 0x10000000
#define PE_CTRL_TOKEN_INTERRUPT_MASK 0x00000001
#define PE_CTRL_FINISH_INTERRUPT_MASK 0x00000002

static bool pi_masked(u32 reg, u32 shift, u32 mask)
{
    return ((reg >> shift) & reg & mask) != 0;
}

static void pi_set_external_interrupt(CPU *cpu, u8 interrupt_source, bool active)
{
    if (active)
        pi_activate_external_interrupt(cpu, interrupt_source);
    else
        pi_deactivate_external_interrupt(cpu, interrupt_source);
}

void pi_update_interrupts(CPU *cpu)
{
    bool di = pi_masked(di_get_disr(), 1, DI_DISR_INTERRUPT_MASK) ||
              pi_masked(di_get_dicvr(), 1, DI_DICVR_INTERRUPT_MASK);

    bool exi = false;
    for (u8 ch = 0; ch < 3; ch++)
        exi |= pi_masked(exi_get_csr(ch), 1, EXI_CSR_INTERRUPT_MASK);

    bool vi = false;
    for (u8 i = 0; i < 4; i++)
        vi |= pi_masked(vi_get_display_interrupt(i), 3, VI_DI_INTERRUPT_MASK);

    pi_set_external_interrupt(cpu, INTERRUPT_SOURCE_DI, di);
    pi_set_external_interrupt(cpu, INTERRUPT_SOURCE_SI,
                              pi_masked(si_get_comcsr(), 1, SI_COMCSR_INTERRUPT_MASK));
    pi_set_external_interrupt(cpu, INTERRUPT_SOURCE_EXI, exi);
    pi_set_external_interrupt(cpu, INTERRUPT_SOURCE_AI,
                              pi_masked(ai_get_aicr(), 1, AI_AICR_INTERRUPT_MASK));
    pi_set_external_interrupt(cpu, INTERRUPT_SOURCE_DSP,
                              pi_masked(dsp_get_csr(), 1, DSP_CSR_INTERRUPT_MASK));
    pi_set_external_interrupt(cpu, INTERRUPT_SOURCE_VI, vi);
    pi_set_external_interrupt(cpu, INTERRUPT_SOURCE_PE_TOKEN,
                              pi_masked(pe_get_ctrl(), 2, PE_CTRL_TOKEN_INTERRUPT_MASK));
    pi_set_external_interrupt(cpu, INTERRUPT_SOURCE_PE_FINISH,
                              pi_masked(pe_get_ctrl(), 2, PE_CTRL_FINISH_INTERRUPT_MASK));
}
