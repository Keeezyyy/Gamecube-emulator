#include "exi.h"
#include "bus/interfaces/interface_utils.h"
#include "bus/interfaces/pi.h"
#include "bus/ipl.h"
#include "core/config/config.h"
#include <_abort.h>
#include <assert.h>
#include <stdio.h>

#define CH0_IPL 1

typedef struct {
    u64 EXInCSR;
    u64 EXInMAR;
    u64 EXInLENGTH;
    u64 EXInCR;
    u64 EXInDATA;
} PACKED RegistersPerChannel;

static struct {
    RegistersPerChannel channels[3];

} exi_registers;

void init_exi(void)
{
    exi_registers.channels[1].EXInCSR = 0;
}
static u32 ch0_dev_1_imm;

u32 exi_get_csr(u8 channel)
{
    return (u32)exi_registers.channels[channel].EXInCSR;
}

u64 exi_read(CPU *cpu, u32 adr, u32 size)
{
    u8 ch_index = (adr - 0xCC006800) / 0x14;
    u32 offset = (adr - 0xCC006800) % 0x14;
    u8 current_chip_selected = (exi_registers.channels[ch_index].EXInCSR >> 4) & 0x5;

    if (ch_index == 2) {
        return 0x0; // for console debugging
    }

    switch (offset) {
    case 0x00:
        return exi_registers.channels[ch_index].EXInCSR;

    case 0x0C:
        return exi_registers.channels[ch_index].EXInCR;
    case 0x10: {
        if (current_chip_selected == CH0_IPL && ch_index == 0)
            return ipl_get_imm();
    }
    }

    assert(!"read exi not implemented");
    return 0;
}

void exi_write(CPU *cpu, u32 adr, u64 val, u32 size)
{
    u8 ch_index = (adr - 0xCC006800) / 0x14;
    u32 offset = (adr - 0xCC006800) % 0x14;
    u8 current_chip_selected = (exi_registers.channels[ch_index].EXInCSR >> 4) & 0x5;

    if (ch_index == 2) {
        return; // for console debugging
    }

    switch (offset) {
    case 0x00: {
        u32 csr = (u32)exi_registers.channels[ch_index].EXInCSR;
        u32 v = (u32)val | (csr & ROMDIS);
        if (ch_index != 0)
            v &= ~ROMDIS;
        int w1cs[] = {EXIINT_OFF, TCINT_OFF, EXTINT_OFF};
        set_register_read_only(&csr, v, w1cs, ARRAY_SIZE(w1cs), EXT);
        exi_registers.channels[ch_index].EXInCSR = csr;
        pi_update_interrupts(cpu);
        return;
    }
    case 0x04:

        if (current_chip_selected == CH0_IPL && ch_index == 0) {
            // ipl write
            set_ipl_dma_adr(val & 0x03FFFFE0);

            return;
        }
        break;

    case 0x08:
        if (current_chip_selected == CH0_IPL && ch_index == 0) {
            // ipl write
            set_ipl_dma_size(val);

            return;
        }
    case 0x10:
        if (current_chip_selected == CH0_IPL && ch_index == 0) {
            // ipl write
            set_ipl_command(val);
            return;
        }

    case 0x0C:
        if (current_chip_selected == CH0_IPL && ch_index == 0 && val & 1) {
            // ipl write
            exi_registers.channels[0].EXInCR = val & 0x3E;

            if (((val >> 1) & 1) == 1) {
                DEBUG_PRINT("[EXI] start IPL dma\n");
                ipl_start_dma_transfer(cpu->bus);
                exi_registers.channels[0].EXInCSR |= TCINT;
                pi_update_interrupts(cpu);
            } else {
                DEBUG_PRINT("[EXI] start IPL imma\n");
                ipl_start_imm_data(cpu->bus);
            }

            return;
        }
    }

    assert(!"exi write");
}
