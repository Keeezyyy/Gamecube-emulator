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
#define RAM_SIZE 0x01800000u /* 24 MB */

static inline u32 be32_load(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static inline void be32_store(u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static inline u32 ram_offset(u32 adr)
{
    u32 off = adr & 0x0fffffff;
    return (off < RAM_SIZE) ? off : RAM_SIZE;
}

static void _write_word(Bus *self, u32 adr, u32 val)
{
    u32 off = ram_offset(adr);
    if (off < RAM_SIZE) {
        be32_store((u8 *)self->ram + off, val);
        return;
    }
    abort();
}

static u32 _read_word(Bus *self, u32 adr)
{
    u32 off = ram_offset(adr);
    if (off < RAM_SIZE)
        return be32_load((const u8 *)self->ram + off);

    if (adr >= 0xfff00000)
        return be32_load((const u8 *)self->ipl + (adr & 0x000fffff));

    assert(!"mem map adr not implemented");
    return 0;
}

static const Bus BUS_TEMPLATE = {
    .load_ipl = _load_ipl,
    .free = &_free,
    .read = &_read,
    .get_ram_location = &_get_ram_location,
    .write = &_write_word,
    .read_word = &_read_word,
};

void init_bus(Bus *self)
{

    *self = BUS_TEMPLATE;

    printf("ram buffer : 0x%016x\n", ram_buffer);

    self->ram = ram_buffer;
}
