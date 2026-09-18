#include "bus.h"
#include "bus/interfaces/di.h"
#include "bus/interfaces/exi.h"
#include "bus/interfaces/mi.h"
#include "bus/interfaces/pi.h"
#include "bus/interfaces/ai.h"
#include "bus/interfaces/si.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static u8 ram_buffer[RAM_SIZE];

static int dol_load_into_ram(Bus *self);

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

    return dol_load_into_ram(self);
}
static void _free(ARG)
{
    free(self->ipl);
}

static inline void be_store(u8 *p, u64 v, u32 size)
{
    for (u32 i = 0; i < size; i++)
        p[i] = (u8)(v >> ((size - 1 - i) * 8));
}

static inline u64 be_load(const u8 *p, u32 size)
{
    u64 v = 0;
    for (u32 i = 0; i < size; i++)
        v = (v << 8) | p[i];
    return v;
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

#define DOL_HEADER_SIZE 0x100u
#define DOL_SECTIONS 18u
#define DOL_OFFSET_TABLE 0x00u
#define DOL_ADDRESS_TABLE 0x48u
#define DOL_SIZE_TABLE 0x90u
#define DOL_BSS_ADDRESS 0xd8u
#define DOL_BSS_SIZE 0xdcu
#define DOL_ENTRY_POINT 0xe0u

static inline u32 be32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static int dol_load_into_ram(Bus *self)
{
    const u8 *const dol = (const u8 *)self->ipl;

    if (self->ipl_size < DOL_HEADER_SIZE) {
        printf("[DOL] image too small : %zu bytes\n", self->ipl_size);
        return 1;
    }

    for (u32 i = 0; i < DOL_SECTIONS; i++) {
        const u32 file_offset = be32(dol + DOL_OFFSET_TABLE + i * 4);
        const u32 adr = be32(dol + DOL_ADDRESS_TABLE + i * 4);
        const u32 size = be32(dol + DOL_SIZE_TABLE + i * 4);

        if (size == 0)
            continue;

        if ((u64)file_offset + size > (u64)self->ipl_size) {
            printf("[DOL] section %u exceeds file : offset 0x%08x size 0x%08x\n", i, file_offset,
                   size);
            return 1;
        }

        const u32 off = ram_offset(adr);
        if (off == RAM_OFFSET_INVALID || (u64)off + size > RAM_SIZE) {
            printf("[DOL] section %u has no ram mapping : adr 0x%08x size 0x%08x\n", i, adr, size);
            return 1;
        }

        DEBUG_PRINT("[DOL] section %u : file 0x%06x -> 0x%08x, size 0x%06x\n", i, file_offset, adr,
                    size);

        memcpy((u8 *)self->ram + off, dol + file_offset, size);
    }

    const u32 bss_adr = be32(dol + DOL_BSS_ADDRESS);
    const u32 bss_size = be32(dol + DOL_BSS_SIZE);
    if (bss_size != 0) {
        const u32 off = ram_offset(bss_adr);
        if (off == RAM_OFFSET_INVALID || (u64)off + bss_size > RAM_SIZE) {
            printf("[DOL] bss has no ram mapping : adr 0x%08x size 0x%08x\n", bss_adr, bss_size);
            return 1;
        }
        memset((u8 *)self->ram + off, 0, bss_size);
    }

    self->entry_point = be32(dol + DOL_ENTRY_POINT);
    DEBUG_PRINT("[DOL] entry point : 0x%08x\n", self->entry_point);

    return 0;
}

static inline bool in_ipl(const Bus *self, u32 adr)
{
    return adr >= IPL_BASE && (u64)(adr - IPL_BASE) < (u64)self->ipl_size;
}

static void _set_cpu_ptr(Bus *self, CPU *cpu)
{
    self->cpu = cpu;
}

static u64 _read(Bus *self, u32 adr, u32 size)
{
    assert(size == 1 || size == 2 || size == 4 || size == 8);

    const u32 off = ram_offset(adr);
    if (off != RAM_OFFSET_INVALID) {
        assert(off + size <= RAM_SIZE);
        return be_load((u8 *)self->ram + off, size);
    }

    if (in_ipl(self, adr)) {
        assert((u64)(adr - IPL_BASE) + size <= (u64)self->ipl_size);
        return be_load((u8 *)self->ipl + (adr - IPL_BASE), size);
    }

    printf("[read2] :  adr : 0x%08x, size : 0x%08x\n", adr, size);
    if (adr >= 0xCC003000 && adr < 0xCC004000) {
        return pi_read(self->cpu, adr, size);

    } else if (adr >= 0xCC004000 && adr < 0xCC005000) {
        assert(!"notaksdlfj aksldfl a");
    } else if (adr >= 0xCC005000 && adr < 0xCC006000) {
        return dsp_read(self->cpu, adr, size);
    } else if (adr >= 0xCC006000 && adr < 0xCC006400) {
        return di_read(self->cpu, adr, size);
    } else if (adr >= 0xCC006400 && adr < 0xCC006800) {
        // si interface
        return si_read(self->cpu, adr, size);
    } else if (adr >= 0xCC006800 && adr < 0xCC006C00) {
        // exi interface
        return exi_read(self->cpu, adr, size);
    } else if (adr >= 0xCC006C00 && adr < 0xCC008000) {
        // reading streaming interface

        if (adr == 0xCC006C00) {

            return ai_read_from_streaming_interface(self->cpu, adr, size);
        }
    }

    DEBUG_PRINT("[BUS] read from unmapped adr : 0x%08x\n", adr);
    assert(!"_read in bus");
    return 0;
}

static void _write(Bus *self, u32 adr, u64 val, u32 size)
{
    assert(size == 1 || size == 2 || size == 4 || size == 8);

    const u32 off = ram_offset(adr);
    if (off != RAM_OFFSET_INVALID) {
        assert(off + size <= RAM_SIZE);
        be_store((u8 *)self->ram + off, val, size);
        return;
    }

    if (in_ipl(self, adr)) {
        DEBUG_PRINT("[BUS] write to IPL rom at 0x%08x ignored\n", adr);
        return;
    }

    printf("[write] : adr : 0x%08x, val : 0x%08x,size : %d\n", adr, val, size);
    if (adr >= 0xCC003000 && adr < 0xCC004000) {
        // pi interface
        pi_write(self->cpu, adr, val, size);
        return;

    } else if (adr >= 0xCC004000 && adr < 0xCC005000) {
        mi_write(self->cpu, adr, val, size);

        return;
    } else if (adr >= 0xCC005000 && adr < 0xCC006000) {
        dsp_write(self->cpu, adr, val, size);
        return;
    } else if (adr >= 0xCC006000 && adr < 0xCC006400) {
        di_write(self->cpu, adr, val, size);
        return;
    } else if (adr >= 0xCC006400 && adr < 0xCC006800) {

        si_write(self->cpu, adr, val, size);
        return;
    } else if (adr >= 0xCC006800 && adr < 0xCC006C00) {
        exi_write(self->cpu, adr, val, size);
        return;
    } else if (adr >= 0xCC006C00 && adr < 0xCC008000) {
        // streming interface

        if (adr == 0xCC006C00) {
            ai_write_to_streaming_interface(self->cpu, adr, val, size);
            return;
        }
    }

    DEBUG_PRINT("[BUS] write to unmapped adr : 0x%08x\n", adr);
    assert(!"_write in bus");
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

    init_exi();
}
