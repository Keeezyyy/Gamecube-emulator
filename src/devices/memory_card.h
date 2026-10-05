#pragma once

#include "bus/interfaces/exi.h"
#define MEMORY_CARD_CHANNEL_NUM 1

typedef enum {
    MEMCARD_SIZE_4MBIT = 4,
    MEMCARD_SIZE_8MBIT = 8,
    MEMCARD_SIZE_16MBIT = 16,
    MEMCARD_SIZE_32MBIT = 32,
    MEMCARD_SIZE_64MBIT = 64,
    MEMCARD_SIZE_128MBIT = 128
} MemCardSize;

typedef enum {
    MEMCARD_LATENCY_4B = 0,
    MEMCARD_LATENCY_8B = 1,
    MEMCARD_LATENCY_16B = 2,
    MEMCARD_LATENCY_32B = 3,
    MEMCARD_LATENCY_64B = 4,
    MEMCARD_LATENCY_128B = 5,
    MEMCARD_LATENCY_256B = 6,
    MEMCARD_LATENCY_512B = 7,
} MemCardLatency;

typedef enum {
    MEMCARD_SECTOR_SIZE_8KB = 0,
    MEMCARD_SECTOR_SIZE_16KB = 1,
    MEMCARD_SECTOR_SIZE_32KB = 2,
    MEMCARD_SECTOR_SIZE_64KB = 3,
    MEMCARD_SECTOR_SIZE_128KB = 4,
    MEMCARD_SECTOR_SIZE_256KB = 5,
} MemCardSectorSize;

typedef struct {
    MemCardSize capacity;
    MemCardSectorSize sector_size;
    MemCardLatency latency;

} MemCard;

void memory_card_exi_transfer(CPU *cpu, RegistersPerChannel *r);
void memory_card_deselect(void);
void init_mem_card(const MemCardSize size);
