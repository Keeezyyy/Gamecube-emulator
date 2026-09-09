#pragma once
#include <stddef.h>
#define RAM_SIZE 0xC0000000 - 0x80000000

#define ARG Bus *self
typedef struct Bus Bus;

struct Bus {
    // RAM 0x80000000 - 0xC0000000
    void *ram;

    void *bios;
    size_t bios_size;

    int (*load_bios)(Bus *self, char *bios_location);
    void (*free)(Bus *self);
};

void init_bus(Bus *self);
