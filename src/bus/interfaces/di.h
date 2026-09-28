#pragma once

#include "cpu/cpu_types.h"
#include "disc/disc.h"
typedef struct {
    char gamename[4];      // 0x00: "GALE"
    char company[2];       // 0x04: "01"
    uint8_t disknum;       // 0x06
    uint8_t gamever;       // 0x07
    uint8_t streaming;     // 0x08: 0 / 1
    uint8_t streambufsize; // 0x09
    uint8_t pad[22 - 4];   // 0x0A - 0x1F
    uint32_t magic;        // 0x20: 0xC2339F3D
} PACKED dvd_disk_id_t;

#define DI_CMD_DVD_AUDIOCONFIG 0xE4
u64 di_read(CPU *cpu, u32 adr, u32 size);
void di_write(CPU *cpu, u32 adr, u32 val, u32 size);
u32 di_get_disr(void);
u32 di_get_dicvr(void);

void di_start_dma(CPU *cpu);

void di_init(Disc *d);
