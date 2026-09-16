#include "bus.h"
#include "bus/interfaces/mi.h"
#include "bus/interfaces/pi.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

static u8 ram_buffer[RAM_SIZE];

static u64 ipl_write_sink;

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
    return (off < RAM_SIZE) ? off : RAM_OFFSET_INVALID;
}

static inline bool in_ipl(const Bus *self, u32 adr)
{
    return adr >= IPL_BASE && (u64)(adr - IPL_BASE) < (u64)self->ipl_size;
}

static void _set_cpu_ptr(Bus *self, CPU *cpu)
{
    self->cpu = cpu;
}

static u64 *_read(Bus *self, u32 adr)
{
    const u32 off = ram_offset(adr);
    if (off != RAM_OFFSET_INVALID)
        return (u64 *)((u8 *)self->ram + off);

    if (in_ipl(self, adr))
        return (u64 *)((u8 *)self->ipl + (adr - IPL_BASE));

    if (adr >= 0xCC003000 && adr < 0xCC004000) {
        return pi_read(adr);

    } else if (adr >= 0xCC004000 && adr < 0xCC005000) {
        assert(!"notaksdlfj aksldfl a");
    }

    DEBUG_PRINT("[BUS] read from unmapped adr : 0x%08x\n", adr);
    assert(!"_read in bus");
    return NULL;
}

static u64 *_write(Bus *self, u32 adr)
{
    const u32 off = ram_offset(adr);
    if (off != RAM_OFFSET_INVALID)
        return (u64 *)((u8 *)self->ram + off);

    if (in_ipl(self, adr)) {
        DEBUG_PRINT("[BUS] write to IPL rom at 0x%08x ignored\n", adr);
        return &ipl_write_sink;
    }

    if (adr >= 0xCC003000 && adr < 0xCC004000) {
        // pi interface
        return pi_write(self->cpu, adr);

    } else if (adr >= 0xCC004000 && adr < 0xCC005000) {
        return mi_write(self->cpu, adr);
    }

    DEBUG_PRINT("[BUS] write to unmapped adr : 0x%08x\n", adr);
    assert(!"_write in bus");
    return NULL;
}

static const Bus BUS_TEMPLATE = {
    .load_ipl = _load_ipl,
    .free = &_free,
    .read = &_read,
    .write = &_write,
    .set_cpu_ptr = _set_cpu_ptr,
};

void init_bus(Bus *self)
{

    *self = BUS_TEMPLATE;

    DEBUG_PRINT("ram buffer : %p\n", (void *)ram_buffer);

    self->ram = ram_buffer;
}
