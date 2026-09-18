#pragma once

#include "bus/bus.h"
#include "core/config/config.h"
#include <assert.h>

#define CMD_SRAM_REQUEST 0x20000100
#define CMD_SECONDS_SINCE_REQUEST 0x20000000

typedef struct OSSram {
    u16 checkSum;
    u16 checkSumInv;
    u32 ead0;
    u32 ead1;
    s32 counterBias;
    s8 displayOffsetH;
    u8 ntd;
    u8 language;
    u8 flags;
    u8 dummy[44];
} PACKED OSSram;

static_assert(sizeof(OSSram) == 64);

void set_ipl_command(u32 cmd);

void set_ipl_dma_adr(u32 adr);
void set_ipl_dma_size(u32 size);

void ipl_start_dma_transfer(Bus *bus);

void ipl_start_imm_data(Bus *bus);
u32 ipl_get_imm(void);
