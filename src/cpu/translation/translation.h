#pragma once

#include "cpu/cpu_types.h"

#include <stddef.h>

typedef struct {
    void *code;
    size_t size;
} TranslationBlockCore;

typedef struct {
    CpuState cpu_state;

    TranslationBlockCore core;
    // for linking tb´s together
    /*
      uint16_t jmp_offset[2];
      uint16_t jmp_insn_offset[2];
      uintptr_t jmp_target_addr[2];
    */
} TranslationBlock;

void init_translation(void);

void deconstruct_translation(void);

TranslationBlock *tb_lookup(CPU *cpu, CpuMode cpu_mode);
