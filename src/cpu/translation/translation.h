#pragma once

#include "cpu/cpu_types.h"
#include "disc/disc.h"

#include <stddef.h>

typedef struct {
    void *code;
    size_t size;
} TranslationBlockCore;

typedef struct {

    TranslationBlockCore core;

    u32 pc_at_start;
    u32 msr_at_start;

    // for linking tb´s together
    /*
      uint16_t jmp_offset[2];
      uint16_t jmp_insn_offset[2];
      uintptr_t jmp_target_addr[2];
    */
} TranslationBlock;

void init_translation(Disc *disc);

void deconstruct_translation(void);

TranslationBlock *tb_lookup(CPU *cpu, CpuMode cpu_mode);
TranslationBlock tb_translate(CPU *cpu, CpuMode cpu_mode);

int tb_finilize(TranslationBlock *tb);

void run_tb(void *code_block);
