#pragma once
#include "core/config/config.h"
#include <stddef.h>
#define RAM_SIZE 0x01800000u
#define IPL_BASE 0xfff00000u

#define ARG Bus *self
typedef struct Bus Bus;

struct Bus {
    // RAM 0x80000000 | 0xC0000000 ram is mirrored to these locations
    void *ram;

    void *ipl;
    size_t ipl_size;

    int (*load_ipl)(Bus *self, char *ipl_location);
    void (*free)(Bus *self);
    u32 (*read)(Bus *self, u32 adr);
    void (*write)(Bus *self, u32 adr, u32 val);
    u32 (*read_word)(Bus *self, u32 adr);
    u64 (*read_dword)(Bus *self, u32 adr);

    u64 (*get_ram_location)(Bus *self);
};

void init_bus(Bus *self);
