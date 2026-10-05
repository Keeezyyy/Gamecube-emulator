#include "exi.h"
#include "bus/interfaces/interface_utils.h"
#include "bus/interfaces/pi.h"
#include "./devices/ipl.h"
#include "core/config/config.h"
#include "devices/memory_card.h"
#include <_abort.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CH0_IPL 0x2
#define CH_MEMCARD 0x1

#define EXI_DMA_ADR_MASK 0x03FFFFE0u

static struct {
    RegistersPerChannel channels[3];

} exi_registers;

void init_exi(void)
{
    // BIT 12 -> External device (Memory card)
    exi_registers.channels[1].EXInCSR = BIT(12);
}
static u32 ch0_dev_1_imm;

static u8 exi_chip_selected(const RegistersPerChannel *r)
{
    return (u8)((r->EXInCSR & CS_MASK) >> CS_OFF);
}

u32 exi_get_csr(u8 channel)
{
    return (u32)exi_registers.channels[channel].EXInCSR;
}

u64 exi_read(CPU *cpu, u32 adr, u32 size)
{
    u8 ch_index = (adr - 0xCC006800) / 0x14;
    u32 offset = (adr - 0xCC006800) % 0x14;
    u8 current_chip_selected = (exi_registers.channels[ch_index].EXInCSR >> 7) & 0x7;

    if (ch_index == 2) {
        return 0x0; // for console debugging
    }

    EXI_PRINT("[EXI] read : adr : 0x%08x\n", adr);

    switch (offset) {
    case 0x00:
        return exi_registers.channels[ch_index].EXInCSR;

    case 0x0C:
        return exi_registers.channels[ch_index].EXInCR;
    case 0x10: {
        if (current_chip_selected == CH0_IPL && ch_index == 0)
            return ipl_get_imm();
        return 0;
    }
    }

    assert(!"read exi not implemented");
    return 0;
}

static u8 *exi_dma_ptr(CPU *cpu, const RegistersPerChannel *r, u32 *len)
{
    u32 mar = (u32)r->EXInMAR & EXI_DMA_ADR_MASK;
    *len = (u32)r->EXInLENGTH & EXI_DMA_ADR_MASK;
    assert((u64)mar + *len <= RAM_SIZE);
    return &cpu->bus->ram[mar];
}

u32 exi_push_data(CPU *cpu, RegistersPerChannel *r, const u8 *data, u32 length)
{
    if (EXI_CR_RW(r->EXInCR) != EXI_RW_READ)
        return 0;

    if (r->EXInCR & EXI_CR_DMA) {
        u32 len;
        u8 *dst = exi_dma_ptr(cpu, r, &len);
        u32 n = MIN(length, len);
        memcpy(dst, data, n);
        memset(dst + n, 0, len - n);
        return n;
    }

    u32 n = MIN(length, EXI_CR_TLEN(r->EXInCR));
    u32 v = 0;
    for (u32 i = 0; i < n; i++)
        v |= (u32)data[i] << (24 - 8 * i);
    r->EXInDATA = v;
    return n;
}

u32 exi_pull_data(CPU *cpu, RegistersPerChannel *r, u8 *data, u32 length)
{
    if (EXI_CR_RW(r->EXInCR) != EXI_RW_WRITE)
        return 0;

    if (r->EXInCR & EXI_CR_DMA) {
        u32 len;
        const u8 *src = exi_dma_ptr(cpu, r, &len);
        u32 n = MIN(length, len);
        memcpy(data, src, n);
        return n;
    }

    u32 n = MIN(length, EXI_CR_TLEN(r->EXInCR));
    for (u32 i = 0; i < n; i++)
        data[i] = (u8)(r->EXInDATA >> (24 - 8 * i));
    return n;
}

static void exi_transfer_finished(CPU *cpu, RegistersPerChannel *r)
{
    r->EXInCSR |= TCINT;
    r->EXInCR &= ~(u64)EXI_CR_TSTART;
    pi_update_interrupts(cpu);
}

void exi_write(CPU *cpu, u32 adr, u64 val, u32 size)
{
    u8 ch_index = (adr - 0xCC006800) / 0x14;
    u32 offset = (adr - 0xCC006800) % 0x14;

    printf("[EXI] write : adr : 0x%08x, val : 0x%08x\n", adr, (u32)val);

    if (ch_index == 2) {
        return; // for console debugging
    }

    RegistersPerChannel *r = &exi_registers.channels[ch_index];
    u8 current_chip_selected = exi_chip_selected(r);
    bool ipl_selected = ch_index == 0 && current_chip_selected == CH0_IPL;

    switch (offset) {
    case 0x00: {
        u32 csr = (u32)r->EXInCSR;
        u32 v = (u32)val | (csr & ROMDIS);
        if (ch_index != 0)
            v &= ~ROMDIS;
        int w1cs[] = {EXIINT_OFF, TCINT_OFF, EXTINT_OFF};
        set_register_read_only(&csr, v, w1cs, ARRAY_SIZE(w1cs), EXT);
        r->EXInCSR = csr;
        if (ch_index == MEMORY_CARD_CHANNEL_NUM && current_chip_selected == CH_MEMCARD &&
            exi_chip_selected(r) != CH_MEMCARD)
            memory_card_deselect();
        pi_update_interrupts(cpu);
        return;
    }
    case 0x04:
        r->EXInMAR = val & EXI_DMA_ADR_MASK;
        if (ipl_selected)
            set_ipl_dma_adr((u32)r->EXInMAR);
        return;

    case 0x08:
        r->EXInLENGTH = val & EXI_DMA_ADR_MASK;
        if (ipl_selected)
            set_ipl_dma_size((u32)r->EXInLENGTH);
        return;

    case 0x0C:
        r->EXInCR = val & 0x3F;
        if (!(val & EXI_CR_TSTART))
            return;

        if (ipl_selected) {
            if (r->EXInCR & EXI_CR_DMA) {
                EXI_PRINT("[EXI] start IPL dma\n");
                ipl_start_dma_transfer(cpu->bus);
            } else {
                EXI_PRINT("[EXI] start IPL imma\n");
                ipl_start_imm_data(cpu->bus);
                if (EXI_CR_RW(r->EXInCR) == EXI_RW_READ)
                    r->EXInDATA = ipl_get_imm();
            }
        } else if (ch_index == MEMORY_CARD_CHANNEL_NUM && current_chip_selected == CH_MEMCARD) {
            memory_card_exi_transfer(cpu, r);
        }

        exi_transfer_finished(cpu, r);
        return;

    case 0x10:
        r->EXInDATA = (u32)val;
        if (ipl_selected)
            set_ipl_command((u32)val);
        return;
    }

    assert(!"exi write");
}
