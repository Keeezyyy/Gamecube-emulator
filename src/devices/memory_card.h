#pragma once

#include "bus/interfaces/exi.h"

typedef enum {
    MEMCARD_SIZE_4MB = 4,
    MEMCARD_SIZE_8MB = 8,
    MEMCARD_SIZE_16MB = 16,
    MEMCARD_SIZE_32MB = 32,
    MEMCARD_SIZE_64MB = 64,
    MEMCARD_SIZE_128MB = 128
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
    MEMCARD_INVALID_SIZE_8KB = 0,
    MEMCARD_INVALID_SIZE_16KB = 1,
    MEMCARD_INVALID_SIZE_32KB = 2,
    MEMCARD_INVALID_SIZE_64KB = 3,
    MEMCARD_INVALID_SIZE_128KB = 4,
    MEMCARD_INVALID_SIZE_256KB = 5,
} MemCardInvalidSize;

typedef struct {
    MemCardSize capacity;
    MemCardInvalidSize invalid_capacity;
    MemCardLatency latency;

} MemCard;

void memory_card_exi_transer(CPU *cpu, RegistersPerChannel *r);
void init_mem_card(const MemCardSize size);
