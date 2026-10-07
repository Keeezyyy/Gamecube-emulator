#include "memory_card.h"
#include "bus/bus.h"
#include "bus/interfaces/exi.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

#define MEMORY_CARD_CMD_GET_ID 0x00

static u8 cmd_buffer[0x100];
static u32 cmd_length;
static bool cmd_executed;

static u8 response[0x100];
static u32 response_length;
static u32 response_pos;

static u32 memory_card_cmd_length(u8 cmd)
{
    switch (cmd) {
    case MEMORY_CARD_CMD_GET_ID:
        return 2;
    }

    printf("[Memory card]: cmd 0x%02x not implemented\n", cmd);
    assert(!"memory card cmd not implemented");
    return 1;
}

static void memory_card_respond_u32(u32 v)
{
    assert(response_length + 4 <= sizeof(response));
    response[response_length++] = (u8)(v >> 24);
    response[response_length++] = (u8)(v >> 16);
    response[response_length++] = (u8)(v >> 8);
    response[response_length++] = (u8)v;
}

static void memory_card_execute(void)
{
    switch (cmd_buffer[0]) {
    case MEMORY_CARD_CMD_GET_ID: {
        u32 id = ((u32)card.capacity & 0xFF) | (((u32)card.latency & 0x7) << 8) |
                 (((u32)card.sector_size & 0x7) << 11);
        printf("writing id : 0x%08x\n", id);
        memory_card_respond_u32(id);
        break;
    }
    }
}

void memory_card_exi_transfer(CPU *cpu, RegistersPerChannel *r)
{
    switch (EXI_CR_RW(r->EXInCR)) {
    case EXI_RW_WRITE:
        cmd_length +=
            exi_pull_data(cpu, r, &cmd_buffer[cmd_length], (u32)sizeof(cmd_buffer) - cmd_length);
        if (!cmd_executed && cmd_length > 0 &&
            cmd_length >= memory_card_cmd_length(cmd_buffer[0])) {
            printf("[Memory card]: cmd 0x%02x\n", cmd_buffer[0]);
            memory_card_execute();
            cmd_executed = true;
        }
        break;
    case EXI_RW_READ:
        response_pos +=
            exi_push_data(cpu, r, &response[response_pos], response_length - response_pos);
        break;
    default:
        assert(!"memory card read-write transfer not implemented");
    }
}

void memory_card_deselect(void)
{
    cmd_length = 0;
    cmd_executed = false;
    response_length = 0;
    response_pos = 0;
}

void init_mem_card(const MemCardSize size)
{
    card.capacity = size;
    card.latency = MEMCARD_LATENCY_4B;
    card.sector_size = MEMCARD_SECTOR_SIZE_8KB;
    memory_card_deselect();
}
