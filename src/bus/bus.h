#pragma once
#include "core/config/config.h"
#include <stddef.h>
#define RAM_SIZE 0x017fffff

#define ARG Bus *self
typedef struct Bus Bus;

struct Bus {
    // RAM 0x80000000 | 0xC0000000 ram is mirrored to these locations
    void *ram;

    void *ipl;
    size_t ipl_size;

    int (*load_ipl)(Bus *self, char *ipl_location);
    void (*free)(Bus *self);

    void *(*read)(Bus *self, u32 adr);
};

void init_bus(Bus *self);
