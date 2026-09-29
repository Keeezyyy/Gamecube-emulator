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
    u32 *reg_usage_bit_map;
} TranslateFuncContext;

typedef u32 *(*translate_fn)(TranslateFuncContext ctx);

extern CPU *static_cpu_ptr;
extern u32 offset_to_cpu;

extern u32 _helper_read_word_from_bus(u32 adr);

u32 *translate_addi_func(TranslateFuncContext ctx);
u32 *translate_lwz_func(TranslateFuncContext ctx);
