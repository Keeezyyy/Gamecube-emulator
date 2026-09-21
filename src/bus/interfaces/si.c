#include "si.h"
#include "bus/interfaces/interface_utils.h"
#include "core/config/config.h"
#include <assert.h>
#include <stdio.h>

static SIRegisters si_regs;
static SIChannel channel_before_vblank[NUM_OF_CHANNELS];

static void start_transfer(void)
{
    u8 transfer_target = (si_regs.SICOMCSR & MASK(1, 2) >> 1);

    SIChannel chnl = si_regs.channel[transfer_target];

    u8 cmd = (chnl.OUTBUF & MASK(16, 23)) >> 16;
    u8 out0 = (chnl.OUTBUF & MASK(8, 15)) >> 8;
    u8 out1 = (chnl.OUTBUF & MASK(0, 7));

    printf("[SI] Transfer startet cmd : 0x%02x, out0 : 0x%02x, out1 : 0x%02x\n", cmd, out0, out1);
}

void si_vblank_trigger(void)
{
    for (int i = 0; i < NUM_OF_CHANNELS; i++) {
        if ((si_regs.SIPOLL >> (3 - i)) & 1) {
            si_regs.channel[i] = channel_before_vblank[i];
            si_regs.SISR &= ~BIT(4 + (8 * i));
        }
    }
}

void si_write(CPU *cpu, u32 adr, u64 val, u32 size)
{
    assert(size == 4);
    assert(adr <= 0xCD0064FF);

    if (adr < SI_POLL_ADR) {
        // write to channel buffers
        const u16 idx = (adr - 0xCD006400) / 0xC;

        if ((si_regs.SIPOLL >> (3 - idx)) & 1) {
            channel_before_vblank[idx].OUTBUF = (u32)val;
            si_regs.SISR |= BIT(4 + (8 * idx));
        } else {
            si_regs.channel[idx].OUTBUF = (u32)val;
        }
        return;
    }

    switch (adr) {
    case SI_POLL_ADR:
        si_regs.SIPOLL = (u32)val;
        return;
    case SI_CR_ADR: {
        const int w1cs[] = {31};
        set_register(&si_regs.SICOMCSR, (u32)val, w1cs, ARRAY_SIZE(w1cs));
        return;
    }
    case SI_SISR_ADR: {
        const int w1cs[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
                            14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};
        set_register(&si_regs.SISR, (u32)val & ~BIT(31), w1cs, ARRAY_SIZE(w1cs));

        if (val >> 31) {
            start_transfer();
        }
        return;
    }
    case SI_EXILK_ADR: {
        si_regs.SIEXILK = (u32)val;
        return;
    }
    default:
        assert(!"not implemented si buffer write \n");
        //... buffer
    }
}

u64 si_read(CPU *cpu, u32 adr, u64 val)
{
    assert(adr <= 0xCD0064FF);

    u32 rel_adr = adr - 0xCD006400;
    if (adr < SI_POLL_ADR) {
        u32 idx = rel_adr / 0x0C;
        u32 offset = (rel_adr % 0x0C) / 0x04;

        if (offset == 0) {
            si_regs.SISR &= ~BIT(5 + (8 * idx));
        }
    }

    return ((u32 *)&si_regs)[rel_adr / 4];
}

u32 si_get_comcsr(void)
{
    return si_regs.SICOMCSR;
}
