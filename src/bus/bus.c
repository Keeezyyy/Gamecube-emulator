#include "bus.h"
#include "core/config/config.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static int _load_bios(Bus *self, char *bios_location)
{
    FILE *f = fopen(bios_location, "rb");

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size);
    if (!buffer) {
        fclose(f);
        return 1;
    }

    size_t bytes_read = fread(buffer, 1, size, f);

    self->bios = (uint8_t *)buffer;
    self->bios_size = bytes_read;

    fclose(f);

    return 0;
}
static void _free(ARG)
{
    free(self->bios);
}

static const Bus BUS_TEMPLATE = {.load_bios = _load_bios, .free = &_free};

static u8 ram_buffer[RAM_SIZE];
void init_bus(Bus *self)
{
    *self = BUS_TEMPLATE;

    self->ram = ram_buffer;
}
