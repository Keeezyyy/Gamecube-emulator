#include "si.h"
#include "bus/interfaces/interface_utils.h"
#include "bus/interfaces/pi.h"
#include "core/config/config.h"
#include <assert.h>
#include <stdio.h>

static SIRegisters si_regs;
static SIChannel channel_before_vblank[NUM_OF_CHANNELS];

static void start_transfer(CPU *cpu)
{

    const u8 transfer_target = (si_regs.SICOMCSR >> 1) & 0b11;

    const SIChannel chnl = si_regs.channel[transfer_target];

    const u8 cmd = (chnl.OUTBUF & MASK(16, 23)) >> 16;
    const u8 out0 = (chnl.OUTBUF & MASK(8, 15)) >> 8;
    const u8 out1 = (chnl.OUTBUF & MASK(0, 7));

    SI_PRINT("[SI] Transfer startet cmd : 0x%02x, chnl : %d, ut0 : 0x%02x, out1 : 0x%02x\n", cmd,
             transfer_target, out0, out1);

    switch (cmd) {
    case SI_CMD_GET_STATUS_ID:
        // si_regs.channel[transfer_target].INBUFH = SI_TYPE_NOT_CONNECTED;
        si_regs.SIIOBUF[0] = SI_TYPE_NOT_CONNECTED;
        break;
    default:
        assert(!"transfer not implemented transfer\n");
    }

    si_regs.SICOMCSR |= BIT(31);
    si_regs.SICOMCSR &= ~BIT(0);

    si_regs.SISR |= BIT(5 + (8 * transfer_target));

    pi_update_interrupts(cpu);
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
    SI_PRINT("[SI] write to adr : 0x%08x, val : 0x%08x\n", adr, val);
    assert(size == 4);
    assert(adr <= 0xCC0064FF && adr >= 0xCC006400);

    u32 rel_adr = adr - 0xCC006400;
    if (adr < SI_POLL_ADR) {
        // write to channel buffers
        const u16 idx = (adr - 0xCC006400) / 0xC;

        if ((si_regs.SIPOLL >> (3 - idx)) & 1) {
            channel_before_vblank[idx].OUTBUF = (u32)val;
            si_regs.SISR |= BIT(4 + (8 * idx));
        } else {
            si_regs.channel[idx].OUTBUF = (u32)val;
        }
        return;
    }
    if (adr >= SI_BUFFER_START_ADR && adr <= SI_BUFFER_END_ADR) {
        si_regs.SIIOBUF[rel_adr / 4] = (u32)val;
        return;
    }

    switch (adr) {
    case SI_POLL_ADR:
        si_regs.SIPOLL = (u32)val;
        return;
    case SI_CR_ADR: {
        int w1cs[] = {31};
        set_register_read_only(&si_regs.SICOMCSR, (u32)val, w1cs, ARRAY_SIZE(w1cs),
                               BIT(28) | BIT(29));
        if (val & 1) {
            // start transfer
            start_transfer(cpu);
        }
        pi_update_interrupts(cpu);

        return;
    }
    case SI_SISR_ADR: {
        const int w1cs[] = {
            24, 16, 8,  0, // SISR_UNDERRUN    0x01
            25, 17, 9,  1, // SISR_OVERRUN     0x02
            26, 18, 10, 2, // SISR_COLLISION   0x04
            27, 19, 11, 3  // SISR_NORESPONSE  0x08
        };
        set_register(&si_regs.SISR, (u32)val & ~BIT(31), w1cs, ARRAY_SIZE(w1cs));

        if (val >> 31) {
            // TODO: find better information about bit 31
            //  start_transfer();
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

u64 si_read(CPU *cpu, u32 adr, u32 size)
{
    assert(adr <= 0xCC0064FF);
    assert(size == 4);

    u32 rel_adr = adr - 0xCC006400;
    if (adr < SI_POLL_ADR) {
        u32 idx = rel_adr / 0x0C;
        u32 offset = (rel_adr % 0x0C) / 0x04;

        if (offset == 0) {
            si_regs.SISR &= ~BIT(5 + (8 * idx));
        }
    }

    SI_PRINT("[SI] READ : 0x%08x, val : 0x%08x\n", adr, ((u32 *)&si_regs)[rel_adr / 4]);

    return ((u32 *)&si_regs)[rel_adr / 4];
}

u32 si_get_comcsr(void)
{
    return si_regs.SICOMCSR;
}
