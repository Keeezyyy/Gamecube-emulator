#pragma once
#include "bus/bus.h"

typedef struct {
    u32 instruction;
    CPU *cpu;
    u32 *pc_after_instruction;
    u32 *pc_buffer;
    u32 *host_block_offsets;
    u32 pc_buffer_counter;
    u32 *code_buffer;
    u32 *code_buffer_end;
    u8 *termination_type;
    u8 *tb_type;
} TranslateFuncContext;

typedef u32 *(*translate_fn)(TranslateFuncContext ctx);

u32 *translate_addi_func(TranslateFuncContext ctx);
