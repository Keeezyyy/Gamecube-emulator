#include "ai.h"
#include "bus/bus.h"
#include "bus/interfaces/interface_utils.h"
#include "bus/interfaces/pi.h"
#include "core/config/config.h"
#include "scheduler/scheduler.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

static u32 dsp_aram = 0;
static u64 DSP_CTRL = 0;

static u16 mailbox_to_dsp_hi;
static u16 mailbox_to_dsp_lo;

static u16 mailbox_from_dsp_hi;
static u16 mailbox_from_dsp_lo;

static u64 audio_interface_control_register = 0;
static u64 audio_interface_sample_counter = 0;

static u32 mmaddr;
static u32 araddr;
static u32 dma_cnt;

static u32 ar_refresh;

static u8 ARAM[MB(ARAM_CAPACITY_IN_MB)];

void dsp_write(CPU *cpu, u32 adr, u32 val, u32 size)
{
    if (adr == DSP_MAIL_TO_DSP_HI) {
        mailbox_to_dsp_hi = val & ~(1 << 15);
        return;

    } else if (adr == DSP_CONTROL) {
        if (BIT_CHECK(DSP_CTRL, 2) != 0 && BIT_CHECK(val, 2) == 0) {
            // dsp halt 1 -> 0
            // expect to recieve data

            // val doesnt matter in init stage
            mailbox_from_dsp_hi = 0xABCD;
        }

        int w1cs[] = {3, 5, 7};
        set_register(&DSP_CTRL, val & ~0x1u, w1cs, ARRAY_SIZE(w1cs));
        return;

    } else if (adr == DSP_AR_INFO) {
        dsp_aram = val & 0x007F;

        return;
    } else if (adr >= 0xCC005020 && adr <= 0xCC005022) {

        if (size == 4) {
            mmaddr = val;
        } else {
            mmaddr &= 0xFFFF << adr == 0xCC005020 ? 2 : 0;
            mmaddr |= val << adr == 0xCC005020 ? 2 : 0;
        }

        return;
    } else if (adr >= 0xCC005024 && adr <= 0xCC005026) {

        if (size == 4) {
            araddr = val;
        } else {
            araddr &= 0xFFFFu << (adr == 0xCC005024 ? 2 : 0);
            araddr |= val << (adr == 0xCC005024 ? 2 : 0);
        }

        return;
    } else if (adr >= 0xCC005028 && adr <= 0xCC00502A) {
        // dma start

        if (size == 4) {
            dma_cnt = val;
        } else {
            dma_cnt &= 0xFFFFu << (adr == 0xCC005028 ? 2 : 0);
            dma_cnt |= val << (adr == 0xCC005028 ? 2 : 0);
        }
        if (size == 4 || adr == 0xCC00502A) {
            // NOTE: dma starts

            bool from_ram_to_aram = (dma_cnt >> 31) == 0 ? true : false;

            assert((dma_cnt & 0x7FFFFFFF) <= MB(ARAM_CAPACITY_IN_MB));

            if (from_ram_to_aram) {
                memcpy(ARAM, cpu->bus->ram, dma_cnt & 0x7FFFFFFF);
            } else {

                memcpy(cpu->bus->ram, ARAM, dma_cnt & 0x7FFFFFFF);
            }

            DSP_CTRL |= (1 << 5);
        }

        return;
    } else if (adr == DSP_AR_REFRESH) {
        ar_refresh = val & 0x07FF;
        return;
    }

    assert(!"sound dsp write");
}

u64 dsp_read(CPU *cpu, u32 adr, u32 size)
{
    assert(size == 2);
    if (adr == 0xCC00500A) {
        return DSP_CTRL;
    } else if (adr == DSP_MAIL_FROM_DSP_HI) {
        return mailbox_from_dsp_hi;
    } else if (adr == DSP_MAIL_FROM_DSP_LO) {
        return mailbox_from_dsp_lo;
    } else if (adr == DSP_AR_REFRESH) {
        return ar_refresh;
    } else if (adr == AR_MODE) {
        return 1;
    } else if (adr == DSP_AR_INFO) {
        return dsp_aram;
    } else if (adr >= 0xCC005020 && adr <= 0xCC005022) {

        if (size == 4) {
            return mmaddr;

        } else {
            return mmaddr >> (adr == 0xCC005022 ? 16 : 0);
        }
    } else if (adr >= 0xCC005024 && adr <= 0xCC005026) {

        if (size == 4) {
            return araddr;

        } else {
            return araddr >> (adr == 0xCC005026 ? 16 : 0);
        }
    } else if (adr >= 0xCC005028 && adr <= 0xCC00502a) {

        if (size == 4) {
            return dma_cnt;

        } else {
            return dma_cnt >> (adr == 0xCC00502a ? 16 : 0);
        }
    }

    printf("[DSP_READ] :  adr : 0x%08x\n", adr);
    assert(!"sound not implemented");
    return 0;
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
