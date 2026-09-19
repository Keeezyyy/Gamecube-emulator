#include "ai.h"
#include "bus/bus.h"
#include "bus/interfaces/interface_utils.h"
#include "bus/interfaces/pi.h"
#include "core/config/config.h"
#include "scheduler/scheduler.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static DSPRegisters dsp = {.CSR = 0x0004, .AR_MODE = 1, .AR_REFRESH = 156};

static u8 ARAM[MB(ARAM_CAPACITY_IN_MB)];

static volatile u16 *dsp_reg(u32 adr)
{
    u32 off = adr - DSP_BASE;
    assert(off + 2 <= sizeof(dsp));
    return (volatile u16 *)((volatile u8 *)&dsp + off);
}

static void dsp_update_interrupt(CPU *cpu)
{
    if ((dsp.CSR >> 1) & dsp.CSR & 0x00A8) {
        pi_activate_external_interrupt(cpu, INTERRUPT_SOURCE_DSP);
    } else {
        pi_deactivate_external_interrupt(cpu, INTERRUPT_SOURCE_DSP);
    }
}

static void dsp_write_csr(CPU *cpu, u16 val)
{
    if ((dsp.CSR & 0x0004) && !(val & 0x0004)) {
        dsp.MAIL_FROM_DSP_HI = 0xABCD;
    }

    if (val & 0x0001) {
        dsp.AUDIO_DMA_CONTROL_LEN = 0;
    }

    dsp.CSR = (u16)((dsp.CSR & ~0x0956u & ~(val & 0x00A8u)) | (val & 0x0956u));
    dsp_update_interrupt(cpu);
}

static void dsp_aram_dma(CPU *cpu)
{
    u32 mm = (u32)dsp.AR_DMA_MMADDR_H << 16 | dsp.AR_DMA_MMADDR_L;
    u32 ar = (u32)dsp.AR_DMA_ARADDR_H << 16 | dsp.AR_DMA_ARADDR_L;
    u32 len = (u32)(dsp.AR_DMA_CNT_H & 0x03FF) << 16 | dsp.AR_DMA_CNT_L;

    assert(mm + len <= RAM_SIZE);
    assert(ar + len <= sizeof(ARAM));

    if (dsp.AR_DMA_CNT_H & 0x8000) {
        memcpy((u8 *)cpu->bus->ram + mm, ARAM + ar, len);
    } else {
        memcpy(ARAM + ar, (u8 *)cpu->bus->ram + mm, len);
    }

    mm += len;
    ar += len;
    dsp.AR_DMA_MMADDR_H = (u16)(mm >> 16);
    dsp.AR_DMA_MMADDR_L = (u16)mm;
    dsp.AR_DMA_ARADDR_H = (u16)(ar >> 16);
    dsp.AR_DMA_ARADDR_L = (u16)ar;
    dsp.AR_DMA_CNT_H &= 0x8000;
    dsp.AR_DMA_CNT_L = 0;

    dsp.CSR |= 0x0020;
    dsp_update_interrupt(cpu);
}

static void dsp_write16(CPU *cpu, u32 adr, u16 val)
{
    switch (adr) {
    case DSP_MAIL_TO_DSP_HI:
        dsp.MAIL_TO_DSP_HI = val;
        return;
    case DSP_MAIL_TO_DSP_LO:
        dsp.MAIL_TO_DSP_LO = val;
        dsp.MAIL_TO_DSP_HI &= 0x7FFF;
        return;
    case DSP_CONTROL:
        dsp_write_csr(cpu, val);
        return;
    case DSP_INTERRUPT_CONTROL:
        dsp.INTERRUPT_CONTROL = val;
        return;
    case DSP_AR_INFO:
        dsp.AR_INFO = val & 0x007F;
        return;
    case DSP_AR_REFRESH:
        dsp.AR_REFRESH = val & 0x07FF;
        return;
    case DSP_AR_DMA_MMADDR_H:
        dsp.AR_DMA_MMADDR_H = val & 0x03FF;
        return;
    case DSP_AR_DMA_MMADDR_L:
        dsp.AR_DMA_MMADDR_L = val & 0xFFE0;
        return;
    case DSP_AR_DMA_ARADDR_H:
        dsp.AR_DMA_ARADDR_H = val & 0x03FF;
        return;
    case DSP_AR_DMA_ARADDR_L:
        dsp.AR_DMA_ARADDR_L = val & 0xFFE0;
        return;
    case DSP_AR_DMA_CNT_H:
        dsp.AR_DMA_CNT_H = val & 0x83FF;
        return;
    case DSP_AR_DMA_CNT_L:
        dsp.AR_DMA_CNT_L = val & 0xFFE0;
        dsp_aram_dma(cpu);
        return;
    case DSP_AUDIO_DMA_START_H:
        dsp.AUDIO_DMA_START_H = val & 0x03FF;
        return;
    case DSP_AUDIO_DMA_START_L:
        dsp.AUDIO_DMA_START_L = val & 0xFFE0;
        return;
    case DSP_AUDIO_DMA_BLOCKS_LENGTH:
        dsp.AUDIO_DMA_BLOCKS_LENGTH = val;
        return;
    case DSP_AUDIO_DMA_CONTROL_LEN:
        dsp.AUDIO_DMA_CONTROL_LEN = val;
        return;
    }

    printf("[DSP_WRITE] : adr : 0x%08x, val : 0x%04x\n", adr, val);
    assert(!"dsp write");
}

static u16 dsp_read16(u32 adr)
{
    u16 val = *dsp_reg(adr);
    if (adr == DSP_MAIL_FROM_DSP_LO) {
        dsp.MAIL_FROM_DSP_HI &= 0x7FFF;
    }
    return val;
}

void dsp_write(CPU *cpu, u32 adr, u32 val, u32 size)
{
    assert(size == 2 || size == 4);

    if (size == 4) {
        dsp_write16(cpu, adr, (u16)(val >> 16));
        dsp_write16(cpu, adr + 2, (u16)val);
        return;
    }
    dsp_write16(cpu, adr, (u16)val);
}

u64 dsp_read(CPU *cpu, u32 adr, u32 size)
{
    assert(size == 2 || size == 4);

    if (size == 4) {
        u32 hi = dsp_read16(adr);
        return hi << 16 | dsp_read16(adr + 2);
    }
    return dsp_read16(adr);
}

static u32 ai_offset(u32 adr, u32 size)
{
    u32 off = adr - 0xCC006C00;
    if (size == 2) {
        off ^= 2;
    }
    return off;
}
static AudioRegs ai_regs;
u64 ai_read(CPU *cpu, u32 adr, u32 size)
{

    volatile u8 *p = (volatile u8 *)&ai_regs + ai_offset(adr, size);
    if (size == 2) {
        return *(volatile u16 *)p;
    }
    return *(volatile u32 *)p;
}

static SchedulerEvent e;
void ai_write(CPU *cpu, u32 adr, u64 val, u32 size)
{

    assert(size == 2 || size == 4);
    u32 off = ai_offset(adr, size);

    volatile u8 *p = (volatile u8 *)&ai_regs + off;

    if (adr == 0xCC006C00 && size == 2) {
        return;
    }
    if (adr == 0xCC006C00 || adr == 0xCC006C02) {
        e.active = val & 1 ? true : false;
        e.clock_speed = (val >> 1) & 1 ? 48000 : 32000;
        ai_regs.AISCNT = (val >> 5) & 1 ? 0 : ai_regs.AISCNT;

        scheduler_edit_event(SCHEDULER_EVENT_AI, e);

        int w1cs[] = {3};
        set_register((u32 *)&ai_regs.AICR, (u32)val, w1cs, ARRAY_SIZE(w1cs));
        if ((val >> 3) & 1) {

            pi_deactivate_external_interrupt(cpu, INTERRUPT_SOURCE_AI);
        }

        return;
    }

    if (size == 2) {
        *(volatile u16 *)p = (u16)val;
    } else {
        *(volatile u32 *)p = val;
    }
}

static void ai_clock(CPU *cpu)
{
    ai_regs.AISCNT++;
    if (ai_regs.AISCNT == ai_regs.AIIT && !(ai_regs.AICR & BIT(4))) {
        ai_regs.AICR |= BIT(3);
        if (ai_regs.AICR & BIT(2))
            pi_activate_external_interrupt(cpu, INTERRUPT_SOURCE_AI);
    }
}

void ai_init(void)
{
    e.active = false;
    e.callback = &ai_clock;
    e.clock_speed = 48000;

    scheduler_add_event_to_buffer(SCHEDULER_EVENT_AI, e);
}
