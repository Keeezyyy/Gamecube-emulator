#include "emit.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/emit_utils.h"
#include "cpu/translation/translation_core_defines.h"
#include <assert.h>
#include <stdbool.h>

#include <stdio.h>

static inline u32 a64_movz(u8 rd, u16 imm16, u8 shift)
{
    return 0xD2800000u | ((u32)shift << 21) | ((u32)imm16 << 5) | rd;
}
static inline u32 a64_movk(u8 rd, u16 imm16, u8 shift)
{
    return 0xF2800000u | ((u32)shift << 21) | ((u32)imm16 << 5) | rd;
}

u32 *emit_load_u64(u32 *out, u8 rd, u64 v)
{
    *out++ = a64_movz(rd, v & 0xFFFF, 0);
    for (u8 s = 1; s < 4; s++) {
        u16 chunk = (v >> (16 * s)) & 0xFFFF;
        if (chunk)
            *out++ = a64_movk(rd, chunk, s);
    }
    return out;
}
static inline u32 a64_movz_w(u8 rd, u16 imm16, u8 hw)
{
    return 0x52800000u | ((u32)(hw & 1) << 21) | ((u32)imm16 << 5) | (rd & 31);
}
static inline u32 a64_movk_w(u8 rd, u16 imm16, u8 hw)
{
    return 0x72800000u | ((u32)(hw & 1) << 21) | ((u32)imm16 << 5) | (rd & 31);
}
static inline u32 a64_movn_w(u8 rd, u16 imm16, u8 hw)
{
    return 0x12800000u | ((u32)(hw & 1) << 21) | ((u32)imm16 << 5) | (rd & 31);
}

u32 *emit_load_u32(u32 *out, u8 rd, u32 v)
{
    printf("v : 0x%08x\n", v);
    const u16 lo = (u16)(v & 0xFFFF);
    const u16 hi = (u16)(v >> 16);

    if (hi == 0) {
        *out++ = a64_movz_w(rd, lo, 0);
    } else if (lo == 0) {
        *out++ = a64_movz_w(rd, hi, 1);
    } else if (hi == 0xFFFF) {
        *out++ = a64_movn_w(rd, (u16)~lo, 0);
    } else if (lo == 0xFFFF) {
        *out++ = a64_movn_w(rd, (u16)~hi, 1);
    } else {
        *out++ = a64_movz_w(rd, lo, 0);
        *out++ = a64_movk_w(rd, hi, 1);
    }
    return out;
}
