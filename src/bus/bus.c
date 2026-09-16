#include "bus.h"
#include "core/config/config.h"
#include <_abort.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

static u8 ram_buffer[RAM_SIZE];
static int _load_ipl(Bus *self, char *ipl_location)
{
    FILE *f = fopen(ipl_location, "rb");
    if (!f) {
        perror(ipl_location);
        return 1;
    }

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

#define RAM_OFFSET_INVALID 0xffffffffu

static inline u32 ram_offset(u32 adr)
{
    switch (adr >> 28) {
    case 0x0:
    case 0x8:
    case 0xc:
        break;
    default:
        return RAM_OFFSET_INVALID;
    }

    const u32 off = adr & 0x0fffffff;
    return (off <= RAM_SIZE - 4) ? off : RAM_OFFSET_INVALID;
}

static inline bool in_ipl(const Bus *self, u32 adr)
{
    return adr >= IPL_BASE && (u64)(adr - IPL_BASE) + 4 <= (u64)self->ipl_size;
}

void _print_write(u32 adr, u32 val)
{

    printf("[WRITE] : writing 0x%08x , to : 0x%08x\n", val, adr);
}

static void _write_word(Bus *self, u32 adr, u32 val)
{
    const u32 off = ram_offset(adr);
    if (off != RAM_OFFSET_INVALID) {
        be32_store((u8 *)self->ram + off, val);
        return;
    }

    if (in_ipl(self, adr)) {
        printf("[BUS] write 0x%08x to IPL rom at 0x%08x ignored\n", val, adr);
        return;
    }

    printf("[BUS] unmapped write 0x%08x to 0x%08x\n", val, adr);
    assert(!"bus write error");
    abort();
}
static u32 _read_word(Bus *self, u32 adr)
{
    if (in_ipl(self, adr))
        return be32_load((const u8 *)self->ipl + (adr - IPL_BASE));

    const u32 off = ram_offset(adr);
    if (off != RAM_OFFSET_INVALID)
        return be32_load((const u8 *)self->ram + off);

    printf("[BUS] unmapped read from 0x%08x\n", adr);
    assert(!"mem map adr not implemented");
    return 0;
}
static u64 _read_double_word(Bus *self, u32 adr)
{
    return ((u64)_read_word(self, adr) << 32) | _read_word(self, adr + 4);
}

static const Bus BUS_TEMPLATE = {
    .load_ipl = _load_ipl,
    .free = &_free,
    .get_ram_location = &_get_ram_location,
    .read = &_read_word,
    .write = &_write_word,
    .read_word = &_read_word,
    .read_dword = &_read_double_word,
};

void init_bus(Bus *self)
{

    *self = BUS_TEMPLATE;

    printf("ram buffer : %p\n", (void *)ram_buffer);

    self->ram = ram_buffer;
}
