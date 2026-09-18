#include "exi.h"
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
    case 0x00:
        exi_registers.channels[ch_index].EXInCSR =
            (val & ~(val & ((1 << 3) | (1 << 11) | (1 << 1)))); // w1c
        return;
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

            if (((val >> 1) & 1) == 1) {
                printf("[EXI] start IPL dma\n");
                ipl_start_dma_transfer(cpu->bus);
            } else {
                printf("[EXI] start IPL imma\n");
                ipl_start_imm_data(cpu->bus);
            }

            return;
        }
    }

    assert(!"exi write");
}
