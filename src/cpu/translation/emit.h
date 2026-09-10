#pragma once
#include "../cpu_types.h"
#include <stdbool.h>

// NOTE: importent it maps to the lower 32 bits
//  guest registers r0 - r29 map to aarch64 x0 - x15 (upper and lower half of the 64 bit registers)
// TODO:
// maybe map all guest registes by using lower and upper half of host 64 bit registers

void emit_cbz_cbnz(EmitedBlock *eb, u8 r1, bool is64, bool branch_on_zero,
                   u16 index_of_offset_emitted_block);
typedef enum {
    STR_POST_INDEX,
    STR_PRE_INDEX,
    STR_UNSIGNED_OFFSET,
} str_mode;
void emit_str(EmitedBlock *eb, u8 rn, u8 rt, bool is64, str_mode mode, i32 imm);
#define PUSH(eb, rt) (emit_str(eb, 31, rt, false, STR_POST_INDEX, -4))

void emit_lsr(EmitedBlock *eb, u8 rd, u8 rs, bool is64, u32 shift);
void emit_asr(EmitedBlock *eb, u8 rd, u8 rn, bool is64, u8 shift);

void emit_and(EmitedBlock *eb, u8 rd, u8 rs, u64 bitmask);

#define SHIFT_TYPE_0 0
#define SHIFT_TYPE_16 1
#define SHIFT_TYPE_32 2
#define SHIFT_TYPE_48 3

// move shifted imm into register
void emit_movz(EmitedBlock *eb, u8 reg, u16 imm, u8 shift_type, bool is64);

void emit_movk(EmitedBlock *eb, u8 reg, u16 imm, u8 type, bool is64);

// move val from rs to rd
void emit_mov(EmitedBlock *eb, u8 rd, u8 rs, bool is64);

void emit_bfi(EmitedBlock *eb, u8 rd, u8 rn, bool is64, u8 lsb, u8 width);
