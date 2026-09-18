#include "ipl.h"
#include "bus/bus.h"
#include <assert.h>
#include <stdio.h>

static u32 current_command;

static u32 physical_adr;
static u32 dma_length;

static OSSram ipl_sram = {
    .checkSum = 0x002C,
    .checkSumInv = 0xFFD0,
    .ead0 = 0,
    .ead1 = 0,
    .counterBias = 0,
    .displayOffsetH = 0,
    .ntd = 0,
    .language = 0,
    .flags = 0x2C,
    .dummy =
        {
            [0x26] = 0xFF,
            [0x27] = 0xFF,
        },
};

void set_ipl_command(u32 cmd)
{
    current_command = cmd;
}

void set_ipl_dma_adr(u32 adr)
{
    physical_adr = adr;
}
void set_ipl_dma_size(u32 size)
{
    dma_length = size;
}

static u32 imm_data;
void ipl_start_imm_data(Bus *bus)
{
    printf("current command : 0x%08x\n", current_command);
    switch (current_command) {
    case CMD_SECONDS_SINCE_REQUEST: {

        imm_data = 0x386D4380;
        return;
    }
    }
}

void ipl_start_dma_transfer(Bus *bus)
{
    switch (current_command) {
    case CMD_SRAM_REQUEST: {
        const OSSram *s = &ipl_sram;
        const u32 a = physical_adr;

        bus->write(bus, a + 0x00, s->checkSum, 2);
        bus->write(bus, a + 0x02, s->checkSumInv, 2);
        bus->write(bus, a + 0x04, s->ead0, 4);
        bus->write(bus, a + 0x08, s->ead1, 4);
        bus->write(bus, a + 0x0C, (u32)s->counterBias, 4);
        bus->write(bus, a + 0x10, (u8)s->displayOffsetH, 1);
        bus->write(bus, a + 0x11, s->ntd, 1);
        bus->write(bus, a + 0x12, s->language, 1);
        bus->write(bus, a + 0x13, s->flags, 1);
        for (u32 i = 0; i < sizeof(s->dummy); i++)
            bus->write(bus, a + 0x14 + i, s->dummy[i], 1);
        return;
    }
    default:
        assert(!"ipl dma assert\n");
    }
}
u32 ipl_get_imm(void)
{
    return imm_data;
}
