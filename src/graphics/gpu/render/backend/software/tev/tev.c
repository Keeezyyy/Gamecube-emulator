#include "tev.h"
#include "../rasterize/rasterize.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/render/texture/texture.h"
#include <_string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <raylib.h>
enum { TEV_R = 0, TEV_G = 1, TEV_B = 2, TEV_A = 3 };

typedef struct {
    s16 rabg[4];
} TevColor;

static TevColor tev_reg_start[4];
static TevColor tev_regs[4];
static TevColor tev_konst[4];

static inline s16 sign_extend11(u32 v)
{
    s32 x = (s32)(v & 0x7FF);
    return (s16)((x & 0x400) ? x - 0x800 : x);
}

void tev_write_color_reg(u8 reg, u32 value)
{
    const int idx = (reg - 0xE0) >> 1;
    TevColor *dst = (value & (1u << 23)) ? &tev_konst[idx] : &tev_reg_start[idx];

    const s16 low = sign_extend11(value);
    const s16 high = sign_extend11(value >> 12);

    if (reg & 1) {
        dst->rabg[TEV_B] = low;
        dst->rabg[TEV_G] = high;
    } else {
        dst->rabg[TEV_R] = low;
        dst->rabg[TEV_A] = high;
    }
}

static RGBA _get_rast_color(const PixelAttributes *p, ColorType t)
{

    RGBA out = {0, 0, 0, 0};

    switch (t) {
    case Color0: {
        memcpy(&out, p->colors[0], sizeof(u8) * 4);
        return out;
        break;
    }

    case Color1: {
        memcpy(&out, p->colors[1], sizeof(u8) * 4);
        return out;
        break;
    }
    case Zero: {
        return out;
        break;
    }

    default:
        assert(!"indirect \n");
    }
}

static RGBA swap_color(u8 swap_index, RGBA source)
{
    RGBA out = {0};
    for (int i = 0; i < 2; i++) {
        const u32 reg = get_bp_register_pointer()[0xF6 + 2 * swap_index + i];
        for (int j = 0; j < 2; j++) {
            const u8 source_channel = (reg >> (2 * j)) & 0x3;

            ((u8 *)&out)[(i * 2) + j] = ((u8 *)&source)[source_channel];
        }
    }
    return out;
}

static const u8 konst_fractions[8] = {255, 223, 191, 159, 128, 96, 64, 32};
static const u8 rabg_idx[4] = {TEV_R, TEV_G, TEV_B, TEV_A};
static s16 _konst_channel(const u8 n)
{
    return tev_konst[n & 3].rabg[rabg_idx[(n - 16) >> 2]];
}

static const u8 rgb_num_shift[] = {12, 8, 4, 0};
static const u8 a_num_shift[] = {13, 10, 7, 4};

static RGBAS16 prev;
static void _get_color_inputs(const u32 ctrl, const u32 alpha_ctrl, RGBAS16 input[4],
                              const PixelAttributes *p, RGBA tex_color, RGBA rast_color,
                              u8 tev_stage, const u32 *bp, const RGBAS16 *konst)
{
    for (int i = 0; i < 4; i++) {
        const u8 color_num = (ctrl >> rgb_num_shift[i]) & 0xF;
        const u8 alpha_num = (alpha_ctrl >> a_num_shift[i]) & 0x7;

        switch (color_num) {
        case 0:
            input[i].r = prev.r;
            input[i].g = prev.g;
            input[i].b = prev.b;
            break;
        case 1:
            input[i].r = prev.a;
            input[i].g = prev.a;
            input[i].b = prev.a;
            break;
        case 2:
        case 4:
        case 6:
            input[i].r = tev_regs[color_num / 2].rabg[rabg_idx[0]];
            input[i].g = tev_regs[color_num / 2].rabg[rabg_idx[1]];
            input[i].b = tev_regs[color_num / 2].rabg[rabg_idx[2]];
            break;
        case 3:
        case 5:
        case 7:
            input[i].r = tev_regs[(color_num - 1) / 2].rabg[rabg_idx[3]];
            input[i].g = tev_regs[(color_num - 1) / 2].rabg[rabg_idx[3]];
            input[i].b = tev_regs[(color_num - 1) / 2].rabg[rabg_idx[3]];
            break;
        case 8:
            input[i].r = tex_color.r;
            input[i].g = tex_color.g;
            input[i].b = tex_color.b;
            break;
        case 9:
            input[i].r = tex_color.a;
            input[i].g = tex_color.a;
            input[i].b = tex_color.a;
            break;
        case 10:
            input[i].r = rast_color.r;
            input[i].g = rast_color.g;
            input[i].b = rast_color.b;
            break;
        case 11:
            input[i].r = rast_color.a;
            input[i].g = rast_color.a;
            input[i].b = rast_color.a;
            break;
        case 12:
            input[i].r = 255;
            input[i].g = 255;
            input[i].b = 255;
            break;
        case 13:
            input[i].r = 128;
            input[i].g = 128;
            input[i].b = 128;
            break;
        case 14:
            input[i].r = konst->r;
            input[i].g = konst->g;
            input[i].b = konst->b;
            break;
        case 15:
            input[i].r = 0;
            input[i].g = 0;
            input[i].b = 0;

            break;
        }
        switch (alpha_num) {
        case 0:
            input[i].a = prev.a;
            break;
        case 1:
        case 2:
        case 3:
            input[i].a = tev_regs[alpha_num].rabg[rabg_idx[3]];
            break;
        case 4:
            input[i].a = tex_color.a;
            break;
        case 5:
            input[i].a = rast_color.a;
            break;
        case 6:
            input[i].a = konst->a;
            break;
        case 7:
            input[i].a = 0;
            break;
        }
    }
}

static void _get_color_constant(const u8 constant_num_color, s16 *r, s16 *g, s16 *b,
                                const u8 constant_num_alpha, s16 *a)
{
    const u8 kc = constant_num_color;
    const u8 ka = constant_num_alpha;

    if (kc < 8) {
        *r = *g = *b = konst_fractions[kc];
    } else if (kc < 12) {
        *r = *g = *b = 0;
    } else if (kc < 16) {
        *r = tev_konst[kc - 12].rabg[TEV_R];
        *g = tev_konst[kc - 12].rabg[TEV_G];
        *b = tev_konst[kc - 12].rabg[TEV_B];
    } else {
        *r = *g = *b = _konst_channel(kc);
    }

    if (ka < 8)
        *a = konst_fractions[ka];
    else if (ka < 16)
        *a = 0;
    else
        *a = _konst_channel(ka);
}

static void shrink_rgbas16(RGBAS16 *v)
{
    v->r = v->r & 0xFF;
    v->g = v->g & 0xFF;
    v->b = v->b & 0xFF;
    v->a = v->a & 0xFF;
}

static void expand_rgba8(RGBAS16 *v)
{
    v->r = v->r + (v->r / 128);
    v->g = v->g + (v->g / 128);
    v->b = v->b + (v->b / 128);
    v->a = v->a + (v->a / 128);
}

static s16 _clamp(s32 t, const CombineConfig *conf)
{
    if (conf->clamp)
        t = t < 0 ? 0 : t > 255 ? 255 : t;
    else
        t = t < -1024 ? -1024 : t > 1023 ? 1023 : t;

    return (s16)t;
}

static u32 _cmp_value(const RGBAS16 *v, const enum CombineScale width)
{
    u32 x = (u8)v->r;
    if (width >= 1)
        x |= (u32)(u8)v->g << 8;
    if (width >= 2)
        x |= (u32)(u8)v->b << 16;
    return x;
}

static bool _compare(const u32 a, const u32 b, const CombineConfig *conf)
{
    return conf->operation == OP_ADD ? a > b : a == b;
}

static s16 _compare_channel(s16 a, s16 b, s16 c, s16 d, bool all, const CombineConfig *conf)
{
    const bool cond = conf->scale == 3 ? _compare((u8)a, (u8)b, conf) : all;
    return _clamp(d + (cond ? c : 0), conf);
}

static s16 _mix(s16 a, s16 b, s16 c, s16 d, const CombineConfig *conf)
{
    static const s16 bias[] = {0, 128, -128};

    s32 t = a * (256 - c) + b * c;

    if (conf->scale == SCALE_X2)
        t *= 2;
    else if (conf->scale == SCALE_X4)
        t *= 4;

    if (conf->scale != SCALE_DIV2)
        t += conf->operation == OP_SUB ? 127 : 128;

    t >>= 8;

    if (conf->operation == OP_SUB)
        t = -t;

    s32 dd = d + bias[conf->bias];
    if (conf->scale == SCALE_X2)
        dd *= 2;
    else if (conf->scale == SCALE_X4)
        dd *= 4;
    t += dd;

    if (conf->scale == SCALE_DIV2)
        t >>= 1;

    return _clamp(t, conf);
}

static void _write_dest(enum CombineDest dest, const u8 ch, const s16 v)
{
    if (dest == DEST_PREV)
        ((s16 *)&prev)[ch] = v;
    else
        tev_regs[dest].rabg[ch] = v;
}

static RGBAS16 tev_calc_core(CombineConfig *color_conf, CombineConfig *alpha_conf, RGBAS16 input[4])
{
    RGBAS16 out = {0};

    for (int i = 0; i < 3; i++)
        shrink_rgbas16(&input[i]);

    const RGBAS16 c = input[2];
    expand_rgba8(&input[2]);

    const RGBAS16 *ia = &input[0];
    const RGBAS16 *ib = &input[1];
    const RGBAS16 *id = &input[3];

    if (color_conf->bias == BIAS_COMPARE) {
        const bool all = _compare(_cmp_value(ia, color_conf->scale),
                                  _cmp_value(ib, color_conf->scale), color_conf);
        out.r = _compare_channel(ia->r, ib->r, c.r, id->r, all, color_conf);
        out.g = _compare_channel(ia->g, ib->g, c.g, id->g, all, color_conf);
        out.b = _compare_channel(ia->b, ib->b, c.b, id->b, all, color_conf);
    } else {
        out.r = _mix(input[0].r, input[1].r, input[2].r, input[3].r, color_conf);
        out.g = _mix(input[0].g, input[1].g, input[2].g, input[3].g, color_conf);
        out.b = _mix(input[0].b, input[1].b, input[2].b, input[3].b, color_conf);
    }

    if (alpha_conf->bias == BIAS_COMPARE) {
        const bool all = _compare(_cmp_value(ia, alpha_conf->scale),
                                  _cmp_value(ib, alpha_conf->scale), alpha_conf);

        out.a = _compare_channel(ia->a, ib->a, c.a, id->a, all, alpha_conf);
    } else {
        out.a = _mix(input[0].a, input[1].a, input[2].a, input[3].a, alpha_conf);
    }

    _write_dest(color_conf->dest, TEV_R, out.r);
    _write_dest(color_conf->dest, TEV_G, out.g);
    _write_dest(color_conf->dest, TEV_B, out.b);
    _write_dest(alpha_conf->dest, TEV_A, out.a);

    return out;
}

void draw_pixel(CPU *cpu, const PixelAttributes *p, u32 x, u32 y, u32 ox, u32 oy)
{
    const u32 *bp = get_bp_register_pointer();
    const u32 gen_mode = bp[0];
    const u32 num_of_steps = ((gen_mode >> 10) & 0xF) + 1;

    memcpy(tev_regs, tev_reg_start, sizeof(tev_regs));
    prev.r = tev_regs[0].rabg[TEV_R];
    prev.g = tev_regs[0].rabg[TEV_G];
    prev.b = tev_regs[0].rabg[TEV_B];
    prev.a = tev_regs[0].rabg[TEV_A];

    for (int i = 0; i < num_of_steps; i++) {
        const u32 tev_order = bp[0x28 + i / 2];
        const bool is_even = i % 2 == 0;

        const u8 swap_index_rast_color = bp[0xC1 + (i * 2)] & 0x3;
        RGBA rast_color = _get_rast_color(p, (tev_order >> TEV_COLOR_CHANNEL(is_even)) & 0x7);
        rast_color = swap_color(swap_index_rast_color, rast_color);

        const u8 swap_index_texture_color = (bp[0xC1 + (i * 2)] >> 2) & 0x3;
        u8 tex_unit = (tev_order >> TEV_ORDER_TEX_MAP(is_even)) & 0x7;
        u8 tex_cords = (tev_order >> TEV_TEX_CORD(is_even)) & 0x7;

        RGBA tex_color = {0};
        if ((tev_order >> TEV_ORDER_ENABLE_TEX(is_even)) & 1) {
            TextureUnit u;
            get_texture_unit_regs(&u, tex_unit, bp);

            tex_color = sample_texture(cpu, u, p->tex[tex_cords], p->lod[tex_unit]);
            tex_color = swap_color(swap_index_texture_color, tex_color);
        }

        // constants
        const u8 color_constant = (bp[0xF6 + (i / 2)] >> (i % 2 == 0 ? 4 : 14)) & 0x1F;
        const u8 alpha_constant = (bp[0xF6 + (i / 2)] >> (i % 2 == 0 ? 9 : 19)) & 0x1F;

        RGBAS16 konst;
        _get_color_constant(color_constant, &konst.r, &konst.g, &konst.b, alpha_constant, &konst.a);

        RGBAS16 input[4] = {0};
        _get_color_inputs(bp[0xC0 + (2 * i)], bp[0xC1 + (2 * i)], input, p, tex_color, rast_color,
                          (u8)i, bp, &konst);

        const u32 reg = bp[0xC0 + (2 * i)];

        CombineConfig conf;
        conf.bias = (reg >> 16) & 0x3;
        conf.operation = (reg >> 18) & 0x1;
        conf.clamp = (reg >> 19) & 0x1;
        conf.scale = (reg >> 20) & 0x3;
        conf.dest = (reg >> 22) & 0x3;

        const u32 alpha_reg = bp[0xC1 + (2 * i)];

        CombineConfig alpha_conf;
        alpha_conf.bias = (alpha_reg >> 16) & 0x3;
        alpha_conf.operation = (alpha_reg >> 18) & 0x1;
        alpha_conf.clamp = (alpha_reg >> 19) & 0x1;
        alpha_conf.scale = (alpha_reg >> 20) & 0x3;
        alpha_conf.dest = (alpha_reg >> 22) & 0x3;

        RGBAS16 c = tev_calc_core(&conf, &alpha_conf, input);

        if (i == num_of_steps - 1) {

            DrawPixel(x + ox - 342, y + oy - 342,
                      (Color){(u8)(c.r < 0 ? 0 : c.r > 255 ? 255 : c.r),
                              (u8)(c.g < 0 ? 0 : c.g > 255 ? 255 : c.g),
                              (u8)(c.b < 0 ? 0 : c.b > 255 ? 255 : c.b), 255});
        }
    }
}
