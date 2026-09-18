#include "ai.h"
#include "bus/interfaces/interface_utils.h"
#include "core/config/config.h"
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

static u8 ARAM[MB(ARAM_CAPACITY_IN_MB)];

void ai_write_to_streaming_interface(CPU *cpu, u32 adr, u64 val, u32 size)
{

    u32 value = (u32)val;

    audio_interface_control_register &= ~(val & ((1 << 3)));
    if (val & (1 << 5)) {
        // 5 SCRESET 1 schreiben = AISCNT auf 0 setzen
        audio_interface_sample_counter = 0;
    }
}

u64 ai_read_from_streaming_interface(CPU *cpu, u32 adr, u32 size)
{
    return audio_interface_control_register;
}

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

        DSP_CTRL = val & ~0x1;
        int w1cs[] = {3, 5, 7};
        set_register(&DSP_CTRL, val, w1cs, ARRAY_SIZE(w1cs));
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
    }

    printf("[DSP_READ] :  adr : 0x%08x\n", adr);
    assert(!"sound not implemented");
    return 0;
}
