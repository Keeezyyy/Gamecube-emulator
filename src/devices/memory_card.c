#include "memory_card.h"
#include "bus/bus.h"
#include "bus/interfaces/exi.h"
#include <assert.h>
#include <stdio.h>

static MemCard card;

struct PACKED card_header {
    u32 serial[0x08];
    u16 device_id;
    u16 size;
    u16 encoding;
    u8 padding[0x1d6];
    u16 chksum1;
    u16 chksum2;
};

#define MEMORY_CARD_READ_MEMORY_ID 0x00

void memory_card_exi_transer(CPU *cpu, RegistersPerChannel *r)
{
    printf("[Memory card]: transer startet with cmd : 0x%08x\n", r->EXInDATA);

    switch (r->EXInDATA) {
    case MEMORY_CARD_READ_MEMORY_ID:
        const u32 id = ((card.capacity) & 0x1F) | ((card.latency & 0x7) << 8) |
                       ((card.invalid_capacity & 0x7) << 11);
        exi_push_data(cpu, r, &id, 4);
        exi_transfer_finished(cpu);
        break;
    default:
        assert(!"memory card cmd not implemented\n");
    }
}

void init_mem_card(const MemCardSize size)
{
    card.capacity = size;
    card.latency = MEMCARD_LATENCY_4B;
    card.invalid_capacity = MEMCARD_INVALID_SIZE_8KB;
}
