#pragma once
#include "../cpu_types.h"
#include <stdbool.h>

// NOTE: importent it maps to the lower 32 bits
//  guest registers r0 - r29 map to aarch64 x0 - x15 (upper and lower half of the 64 bit registers)
// TODO:
// maybe map all guest registes by using lower and upper half of host 64 bit registers

#define EMIT_STANDART_PARAMS                                                                       \
    &emitted_block, emitted_block.host_instruction_ *buffer[host_instruction_counter],             \
        &emitted_block.host_instruction_ *buffer[host_instruction_counter].num_of_instructions
#define SHIFT_TYPE_0 0
#define SHIFT_TYPE_16 1
#define SHIFT_TYPE_32 2
#define SHIFT_TYPE_48 3
typedef enum {
    STR_POST_INDEX,
    STR_PRE_INDEX,
    STR_UNSIGNED_OFFSET,
} str_mode;
void emit_str(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rn, u8 rt, bool is64, str_mode mode, i32 imm);
void emit_cbz_cbnz(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter,
                   u64 id, u8 r1, bool is64, bool branch_on_zero, u64 host_instruction_block_id);

void emit_lsr(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rd, u8 rs, bool is64, u32 shift);
void emit_asr(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rd, u8 rn, bool is64, u8 shift);

void emit_and(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rd, u8 rs, u64 bitmask);

// move shifted imm into register
void emit_movz(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter,
               u64 id, u8 reg, u16 imm, u8 shift_type, bool is64);

void emit_movk(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter,
               u64 id, u8 reg, u16 imm, u8 type, bool is64);

// move val from rs to rd
void emit_mov(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rd, u8 rs, bool is64);

void emit_bfi(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rd, u8 rn, bool is64, u8 lsb, u8 width);
#define LDR_POST_INDEX STR_POST_INDEX
#define LDR_PRE_INDEX STR_PRE_INDEX
#define LDR_UNSIGNED_OFFSET STR_UNSIGNED_OFFSET
typedef str_mode ldr_mode;
void emit_ldr(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rn, u8 rt, bool is64, ldr_mode mode, i32 imm);

#define PUSH(eb, buffer, host_instruction_counter, id, rt)                                         \
    (emit_str(eb, buffer, host_instruction_counter, id, 31, rt, false, STR_POST_INDEX, -4))

#define LOAD_64_BIT_IMM(...) LOAD_64_BIT_IMM_IMPL(__VA_ARGS__)
#define LOAD_64_BIT_IMM_IMPL(out, buffer, host_instruction_counter, id, imm, reg)                  \
    emit_movz(out, buffer, host_instruction_counter, id, (reg), (imm) & 0xFFFF, SHIFT_TYPE_0,      \
              true);                                                                               \
    emit_movk(out, buffer, host_instruction_counter, id, (reg), ((imm) >> 16) & 0xFFFF,            \
              SHIFT_TYPE_16, true);                                                                \
    emit_movk(out, buffer, host_instruction_counter, id, (reg), ((imm) >> 32) & 0xFFFF,            \
              SHIFT_TYPE_32, true);                                                                \
    emit_movk(out, buffer, host_instruction_counter, id, (reg), ((imm) >> 48) & 0xFFFF,            \
              SHIFT_TYPE_48, true);

#define GUEST_LOAD_32_BIT_IMM(...) GUEST_LOAD_32_BIT_IMM_IMPL(__VA_ARGS__)
#define GUEST_LOAD_32_BIT_IMM_IMPL(out, buffer, host_instruction_counter, id, imm, reg)            \
    if ((reg) % 2 == 0) {                                                                          \
        emit_movz(out, buffer, host_instruction_counter, (id), (reg), (imm) & 0xFFFF,              \
                  SHIFT_TYPE_0, false);                                                            \
        emit_movk(out, buffer, host_instruction_counter, (id) + 1, (reg), ((imm) >> 16) & 0xFFFF,  \
                  SHIFT_TYPE_16, false);                                                           \
    } else {                                                                                       \
        emit_movk(out, buffer, host_instruction_counter, (id), ((reg) - 33) / 2, (imm) & 0xFFFF,   \
                  SHIFT_TYPE_32, true);                                                            \
        emit_movk(out, buffer, host_instruction_counter, (id) + 1, ((reg) - 33) / 2,               \
                  ((imm) >> 16) & 0xFFFF, SHIFT_TYPE_48, true);                                    \
    }
#define EMIT_STANDART_PARAMS                                                                       \
    &emitted_block,                                                                                \
        &emitted_block.host_instruction_buffer[emitted_block.num_of_host_instrucion_blocks],       \
        &emitted_block.host_instruction_buffer[emitted_block.num_of_host_instrucion_blocks++]      \
             .num_of_instructions
