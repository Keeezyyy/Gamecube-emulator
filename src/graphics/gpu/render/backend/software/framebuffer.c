#include "bus/bus.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "framebuffer.h"
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

static u8 efb[0x1EF000];

static u32 get_fmt(void)
{
    return get_bp_register_pointer()[0x43] & 0x7;
}

static u32 get_zfmt(void)
{
    return (get_bp_register_pointer()[0x43] >> 3) & 0x7;
}

static bool in_bounds(u32 fmt, u32 x, u32 y)
{
    const u32 height = (fmt == PF_RGB565_Z16) ? EFB_HEIGHT_AA : EFB_HEIGHT;
    return x < EFB_WIDTH && y < height;
}

static u32 read_24(u32 offset, u32 x, u32 y)
{
    const u8 *p = efb + offset + (y * EFB_WIDTH + x) * 3;
    return (u32)p[0] << 16 | (u32)p[1] << 8 | p[2];
}

static void write_24(u32 offset, u32 x, u32 y, u32 v)
{
    u8 *p = efb + offset + (y * EFB_WIDTH + x) * 3;
    p[0] = (u8)(v >> 16);
    p[1] = (u8)(v >> 8);
    p[2] = (u8)v;
}

static inline u8 expand(u32 v, u32 bits)
{
    return (u8)((v << (8 - bits)) | (v >> (2 * bits - 8)));
}

static void z16_layout(u32 zfmt, u32 *exp_bits, u32 *mant_bits)
{
    switch (zfmt) {
    case ZC_NEAR:
        *exp_bits = 2;
        *mant_bits = 14;
        break;
    case ZC_MID:
        *exp_bits = 3;
        *mant_bits = 13;
        break;
    case ZC_FAR:
        *exp_bits = 4;
        *mant_bits = 12;
        break;
    default:
        *exp_bits = 0;
        *mant_bits = 16;
        break;
    }
}

static u32 z16_compress(u32 z, u32 zfmt)
{
    u32 e, m;
    z16_layout(zfmt, &e, &m);
    if (e == 0) {
        return z >> 8;
    }

    const u32 max_exp = (1u << e) - 1;
    u32 n = 0;
    while (n < max_exp && (z >> (23 - n)) & 1) {
        n++;
    }

    const u32 consumed = n + (n < max_exp ? 1 : 0);
    const u32 mant = ((z << consumed) & 0xFFFFFF) >> (24 - m);
    return n << m | mant;
}

static u32 z16_decompress(u32 c, u32 zfmt)
{
    u32 e, m;
    z16_layout(zfmt, &e, &m);
    if (e == 0) {
        return (c & 0xFFFF) << 8;
    }

    const u32 max_exp = (1u << e) - 1;
    const u32 n = (c >> m) & max_exp;
    const u32 mant = c & ((1u << m) - 1);
    const u32 consumed = n + (n < max_exp ? 1 : 0);
    const u32 ones = n ? ((1u << n) - 1) << (24 - n) : 0;
    return ones | ((mant << (24 - m)) >> consumed);
}

u32 get_z_in_fb(CPU *cpu, s32 x, s32 y)
{
    (void)cpu;
    const u32 fmt = get_fmt();
    const u32 ux = (u32)x & 0x3FF;
    const u32 uy = (u32)y & 0x3FF;

    if (!in_bounds(fmt, ux, uy)) {
        return 0;
    }

    const u32 z = read_24(EFB_COLOR_BYTES, ux, uy);

    switch (fmt) {
    case PF_RGB8_Z24:
    case PF_RGBA6_Z24:
    case PF_Z24:
        return z;

    case PF_RGB565_Z16:
        return z16_decompress(z, get_zfmt());

    case PF_Y8:
    case PF_U8:
    case PF_V8:
    case PF_YUV420:
    default:
        assert(!"fb format not implemented\n");
        return z;
    }
}

void write_to_fb(s32 x, s32 y, u8 r, u8 g, u8 b, u32 z)
{
    const u32 fmt = get_fmt();
    const u32 ux = (u32)x & 0x3FF;
    const u32 uy = (u32)y & 0x3FF;

    if (!in_bounds(fmt, ux, uy)) {
        return;
    }

    z &= 0xFFFFFF;

    const u32 cmode0 = get_bp_register_pointer()[0x41];
    const bool color_update = (cmode0 >> 3) & 1;
    const bool alpha_update = (cmode0 >> 4) & 1;
    const u8 a = 0xFF;

    switch (fmt) {
    case PF_RGB8_Z24:
    case PF_Z24:
        if (color_update)
            write_24(0, ux, uy, (u32)r << 16 | (u32)g << 8 | b);
        write_24(EFB_COLOR_BYTES, ux, uy, z);
        break;

    case PF_RGBA6_Z24: {
        u32 c = read_24(0, ux, uy);
        if (color_update)
            c = (c & 0x3F) | (u32)(r >> 2) << 18 | (u32)(g >> 2) << 12 | (u32)(b >> 2) << 6;
        if (alpha_update)
            c = (c & ~0x3Fu) | (a >> 2);
        write_24(0, ux, uy, c);
        write_24(EFB_COLOR_BYTES, ux, uy, z);
        break;
    }

    case PF_RGB565_Z16:
        if (color_update)
            write_24(0, ux, uy, (u32)(r >> 3) << 11 | (u32)(g >> 2) << 5 | (b >> 3));
        write_24(EFB_COLOR_BYTES, ux, uy, z16_compress(z, get_zfmt()));
        break;

    case PF_Y8:
    case PF_U8:
    case PF_V8:
    case PF_YUV420:
    default:
        assert(!"fb format not implemented\n");
        break;
    }
}

RGBA read_from_fb(s32 x, s32 y)
{
    const u32 fmt = get_fmt();
    const u32 ux = (u32)x & 0x3FF;
    const u32 uy = (u32)y & 0x3FF;

    RGBA out = {0, 0, 0, 0xFF};
    if (!in_bounds(fmt, ux, uy)) {
        assert(!"should not happen\n");
        return out;
    }

    const u32 c = read_24(0, ux, uy);

    switch (fmt) {
    case PF_RGB8_Z24:
    case PF_Z24:
        out.r = (u8)(c >> 16);
        out.g = (u8)(c >> 8);
        out.b = (u8)c;
        break;

    case PF_RGBA6_Z24:
        out.r = expand((c >> 18) & 0x3F, 6);
        out.g = expand((c >> 12) & 0x3F, 6);
        out.b = expand((c >> 6) & 0x3F, 6);
        out.a = expand(c & 0x3F, 6);
        break;

    case PF_RGB565_Z16:
        out.r = expand((c >> 11) & 0x1F, 5);
        out.g = expand((c >> 5) & 0x3F, 6);
        out.b = expand(c & 0x1F, 5);
        break;

    case PF_Y8:
    case PF_U8:
    case PF_V8:
    case PF_YUV420:
    default:
        assert(!"fb format not implemented\n");
        break;
    }

    return out;
}
