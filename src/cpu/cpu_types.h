#pragma once

#include "bus/bus.h"
#include "core/config/config.h"
#include <stdbool.h>
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

} PACKED CpuRegisters;

typedef struct {
    u32 val;

} CpuMode;
typedef union {
    struct PACKED {
        u32 spr0_7[8];
        u32 lr;
        u32 ctr;
        u32 spr10_919[910];
        u32 hid2;
        u32 spr921_1009[89];
        u32 iabr;
        u32 spr1011_1012[2];
        u32 dabr;
        u32 spr1014_1023[10];
    };

    u32 buf[1024];
} PACKED CpuSpecialPurposeRegisters;

typedef struct CPU CPU;

enum FPRPrecision {
    FPR_PRECISION_DOUBLE = 0,
    FPR_PRECISION_SINGLE = 1,
};

typedef struct {

    u8 (*get_pse_bit)(CPU *self);
    // ps0 - bei HID2[PSE] = 0 der gewoehnliche FPR-Inhalt
    double fpr[32];
    // ps1 - wie beim Gekko intern als double abgelegt, nicht als single
    double ps1[32];

} PACKED FPU;

typedef struct {
    // Upper 32 Bits are ignored
    u64 interrupt_mask_register; //
    u64 interrupt_source_register;
} CpuExcpetion;

struct CPU {
    CpuState state;
    CpuRegisters registers;
    CpuRegisters registers_scratch;
    CpuSpecialPurposeRegisters special_purpose_registers;
    CpuState state_on_start_of_tb;
    CpuExcpetion exception;

    u32 reserve;

    u64 helper_functions[64];

    Bus *bus;
    FPU fpu;
    FPU fpu_scratch;
    u64 fp_args[8];

    void (*main)(CPU *self);
    void (*boot)(CPU *self);
    void (*free)(CPU *self);
    void (*print_state)(CPU *self);
    bool (*awaiting_interrupt)(CPU *self);
    CpuMode (*get_current_cpu_mode)(CPU *self);
};
