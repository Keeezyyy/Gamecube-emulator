#include "exi.h"
#include "core/config/config.h"
#include <assert.h>

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

u64 exi_read(CPU *cpu, u32 adr, u32 size)
{
    u8 ch_index = (adr - 0xCC006800) / 0x14;
    u32 offset = (adr - 0xCC006800) % 0x14;

    switch (offset) {
    case 0x00:
        return exi_registers.channels[ch_index].EXInCSR;

    case 0x0C:
        return exi_registers.channels[ch_index].EXInCR;
    }

    assert(!"read exi not implemented");
    return 0;
}

void exi_write(CPU *cpu, u32 adr, u64 val, u32 size)
{

    u32 value = (u32)val;
    if (adr < 0xCC006814) {
        if (adr == 0xCC006800 + 0x0) {
            exi_registers.channels[0].EXInCSR &= ~(val & ((1 << 3) | (1 << 11) | (1 << 1))); // w1c
        }

    } else if (adr < 0xCC006828) {

    } else {
    }
}
