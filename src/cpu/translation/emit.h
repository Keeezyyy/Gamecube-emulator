#pragma once
#include "../cpu_types.h"

// NOTE: importent it maps to the lower 32 bits
//  guest registers r0 - r29 map to aarch64 x0 - x15 (upper and lower half of the 64 bit registers)
// TODO:
// maybe map all guest registes by using lower and upper half of host 64 bit registers

void emit_cbz(EmitedBlock *eb, u8 r1, u16 index_of_offset_emitted_block);

typedef enum {
    STR_POST_INDEX,
    STR_PRE_INDEX,
    STR_UNSIGNED_OFFSET,
} str_mode;
void emit_str(EmitedBlock *eb, u8 rn, u8 rt, bool is64, str_mode mode, i32 imm);
#define PUSH(eb, rt) (emit_str(eb, 31, rt, false, STR_POST_INDEX, -4))
