#pragma once
#include "../cpu_types.h"
#include <stdbool.h>
u32 *emit_load_u64(u32 *out, u8 rd, u64 v);

u32 *emit_load_u32(u32 *out, u8 rd, u32 v);

u32 *emit_store_u32(u32 *out, u8 rt, u8 rn, s32 off);

typedef enum {
    A64_EXT_UXTW = 0b010, /* Wm, zero-extended  */
    A64_EXT_LSL = 0b011,  /* Xm, kein Extend    */
    A64_EXT_SXTW = 0b110, /* Wm, sign-extended  */
    A64_EXT_SXTX = 0b111, /* Xm, sign-extended  */
} a64_extend;
u32 *emit_store_u32_indexed(u32 *out, u8 rt, u8 rn, u8 rm, a64_extend ext, u8 shift);
