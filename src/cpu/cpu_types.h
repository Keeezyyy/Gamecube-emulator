#pragma once

#include "bus/bus.h"
#include "core/config/config.h"
#include <stddef.h>

typedef struct PACKED {
    u32 pc;
    u32 cr;
    u32 xer;
    u32 fpscr;
    u32 msr;

} CpuState;

typedef struct PACKED {
    u32 gpio[32];
    u32 sr[16];
    f32 fpr[32];

} CpuRegisters;

typedef struct {
    u32 val;

} CpuMode;

typedef struct CPU CPU;

struct CPU {
    CpuState state;
    CpuRegisters registers;

    Bus *bus;

    void (*main)(CPU *self);
    void (*boot)(CPU *self);
    void (*free)(CPU *self);
    CpuMode (*get_current_cpu_mode)(CPU *self);
};

typedef struct {
    // TODO: do this
    //  something like patch patch bits 0-31 in *void with offset to adr of emitted block[5] for the
    //  jump instructions type ...

} EmitPatch;

#define MAX_PATCHES_PER_EMIT_BLOCK 8

typedef struct {
    void *block;
    u32 size;

    EmitPatch patch_ptr[MAX_PATCHES_PER_EMIT_BLOCK];
    u32 num_of_patches;

} PACKED EmitedBlock;

typedef struct {
    bool is_terminating_instruction;

    EmitedBlock *emmited_blocks_ptr;
    u32 num_of_emmited_blocks;
} HostArchOutput;
