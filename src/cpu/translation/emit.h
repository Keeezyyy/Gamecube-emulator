#pragma once
#include "../cpu_types.h"

// guest registers r0 - r29 map to aarch64 x0 - x29
//                 r30- r31 map to memory
//

inline void emit_cbz(EmitedBlock *eb, u32 *counter, u8 r1, u32 offset);

typedef enum {
    STR_POST_INDEX,
    STR_PRE_INDEX,
    STR_UNSIGNED_OFFSET,
} str_mode;

inline void emit_str(EmitedBlock *eb, u32 *counter, u8 rn, u8 rt, bool is64, str_mode mode,
                     i32 imm);
