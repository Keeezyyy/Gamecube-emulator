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
typedef union {
    struct {
        u32 spr0_7[8];
        u32 lr;
        u32 spr9_919[911];
        u32 hid2;
        u32 spr921_1009[89];
        u32 iabr;
        u32 spr1011_1012[2];
        u32 dabr;
        u32 spr1014_1023[10];
    };

    u32 buf[1024];
} CpuSpecialPurposeRegisters;

typedef struct CPU CPU;

struct CPU {
    CpuState state;
    CpuRegisters registers;
    CpuSpecialPurposeRegisters special_purpose_registers;

    CpuState state_on_start_of_tb;

    Bus *bus;

    void (*main)(CPU *self);
    void (*boot)(CPU *self);
    void (*free)(CPU *self);
    CpuMode (*get_current_cpu_mode)(CPU *self);
};

enum PatchingType {
    OFFSET_TO_EMITTED_BLOCK,
    ADDRESS_TO_EMITTED_BLOCK

};

// NOTE: the offset is in 4 byte jumps
typedef struct {
    // TODO: do this
    //  something like patch patch bits 0-31 in *void with offset to adr of emitted block[5] for the
    //  jump instructions type ...

    enum PatchingType type;
    u32 *instruction_ptr;

    u32 bit_mask;
    u16 emitted_block_index;
    u8 bit_start;
    u8 bit_end;
    i8 bit_shift;

} EmitPatch;

#define MAX_PATCHES_PER_EMIT_BLOCK 8
#define MAX_STATIC_INSTRUCTIONS_PER_EMIT_BLOCK 64

// the output of 1 translated guest instruction
typedef struct {
    u32 host_code_buffer[64];
    u32 num_of_host_instructions;

    EmitPatch patch_ptr[MAX_PATCHES_PER_EMIT_BLOCK];
    u32 num_of_patches;

} PACKED EmitedBlock;

typedef struct {
    bool is_terminating_instruction;

    EmitedBlock *emmited_blocks_ptr;
    u32 num_of_emmited_blocks;
} HostArchOutput;
