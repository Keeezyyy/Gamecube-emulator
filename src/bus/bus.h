#pragma once
#include "core/config/config.h"
#include <stddef.h>
#define RAM_SIZE 0x01800000u
#define IPL_BASE 0xfff00000u

#define ARG Bus *self
typedef struct Bus Bus;
// forward declaration, cpu_types.h includes bus.h
typedef struct CPU CPU;

struct Bus {
    // RAM 0x80000000 | 0xC0000000 ram is mirrored to these locations
    u8 *ram;

    void *ipl;
    size_t ipl_size;

    // echtes Bootrom (ipl.bin), Quelle fuer EXI-ROM-Reads (z.B. Fonts)
    void *ipl_rom;
    size_t ipl_rom_size;

    // Einsprungpunkt des geladenen Images (aus dem DOL-Header)
    u32 entry_point;

    CPU *cpu;

    int (*load_ipl)(Bus *self, char *ipl_location);
    int (*load_ipl_scrambled)(Bus *self, char *ipl_location);
    void (*free)(Bus *self);
    void (*set_cpu_ptr)(Bus *self, CPU *cpu);
    u64 (*read)(Bus *self, u32 adr, u32 size);
    // val is the guest value in host byte order, size in bytes (1, 2, 4 or 8)
    void (*write)(Bus *self, u32 adr, u64 val, u32 size);
};

void init_bus(Bus *self);
