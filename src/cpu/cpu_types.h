#pragma once

#include "core/config/config.h"

typedef struct PACKED {
    u64 pc;
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

    void (*main)(CPU *self);
    void (*free)(CPU *self);
    CpuMode (*get_current_cpu_mode)(CPU *self);
};
