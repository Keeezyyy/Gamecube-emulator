#include "bus.h"
#include "core/config/config.h"
#include <_abort.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static u8 ram_buffer[RAM_SIZE];
static int _load_ipl(Bus *self, char *ipl_location)
{
    FILE *f = fopen(ipl_location, "rb");

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size);
    if (!buffer) {
        fclose(f);
        return 1;
    }

    size_t bytes_read = fread(buffer, 1, size, f);

    self->ipl = (uint8_t *)buffer;
    self->ipl_size = bytes_read;

    fclose(f);

    return 0;
}
static void _free(ARG)
{
    free(self->ipl);
}
static u64 _get_ram_location(ARG)
{
    return (u64)ram_buffer;
}

static void *_read(Bus *self, u32 adr)
{
    // mirrored ram area
    /*
    0x00000000 0x017fffff 24mb physical address of the ram
    0x80000000 0x817fffff 24mb logical address of the ram, cached
    0xc0000000 0xc17fffff 24mb logical address of the ram, not cached
    * */
    if (adr < 0xc17fffff) {
        return &ram_buffer[adr & 0x0fffffff];
    } else if (false) {
        // todo: rest of the memory map
    } else {
        return &((uint8_t *)self->ipl)[adr & 0x000fffff];
    }
}
static void _write(ARG, u32 adr, u32 val)
{
    if (adr < 0xC17fffff) {
        // ram area
        ((u32 *)self->ram)[adr & 0x0fffffff] = val;
    } else {
        assert(!"bus adr not implemented ");
    }
}

static const Bus BUS_TEMPLATE = {.load_ipl = _load_ipl,
                                 .free = &_free,
                                 .read = &_read,
                                 .get_ram_location = &_get_ram_location,
                                 .write = &_write};

void init_bus(Bus *self)
{

    *self = BUS_TEMPLATE;

    printf("ram buffer : 0x%016x\n", ram_buffer);

    self->ram = ram_buffer;
}
