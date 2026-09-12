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

static inline u32 a64_str_w_uimm(u8 rt, u8 rn, u32 imm12)
{
    return 0xB9000000u | ((imm12 & 0xFFFu) << 10) | ((u32)(rn & 31) << 5) | (rt & 31);
}

static inline u32 a64_stur_w(u8 rt, u8 rn, s32 imm9)
{
    return 0xB8000000u | (((u32)imm9 & 0x1FFu) << 12) | ((u32)(rn & 31) << 5) | (rt & 31);
}

u32 *emit_store_u32(u32 *out, u8 rt, u8 rn, s32 off)
{
    if (off >= 0 && off <= 16380 && (off & 3) == 0)
        *out++ = a64_str_w_uimm(rt, rn, (u32)off >> 2);
    else if (off >= -256 && off <= 255)
        *out++ = a64_stur_w(rt, rn, off);
    else
        assert(0 && "emit_store_u32: offset needs scratch register");
    return out;
}

static inline u32 a64_str_w_reg(u8 rt, u8 rn, u8 rm, a64_extend ext, bool shift)
{
    return 0xB8200800u | ((u32)(rm & 31) << 16) | ((u32)ext << 13) | ((u32)shift << 12) |
           ((u32)(rn & 31) << 5) | (rt & 31);
}

u32 *emit_store_u32_indexed(u32 *out, u8 rt, u8 rn, u8 rm, a64_extend ext, u8 shift)
{
    assert((shift == 0 || shift == 2) && "str w: Shift darf nur 0 oder 2 sein");
    assert(
        (ext == A64_EXT_UXTW || ext == A64_EXT_SXTW || ext == A64_EXT_LSL || ext == A64_EXT_SXTX) &&
        "ungültige Extend-Option");
    *out++ = a64_str_w_reg(rt, rn, rm, ext, shift == 2);
    return out;
}
