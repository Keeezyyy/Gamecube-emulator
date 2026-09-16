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
    void *ram;

    void *ipl;
    size_t ipl_size;

    CPU *cpu;

    int (*load_ipl)(Bus *self, char *ipl_location);
    void (*free)(Bus *self);
    void (*set_cpu_ptr)(Bus *self, CPU *cpu);
    u64 *(*read)(Bus *self, u32 adr);
    u64 *(*write)(Bus *self, u32 adr);
};

void init_bus(Bus *self);
