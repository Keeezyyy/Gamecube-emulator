#include "tev.h"
#include "../rasterize/rasterize.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/render/texture/texture.h"
#include <_string.h>
#include <assert.h>
#include <string.h>

#include <raylib.h>
enum { TEV_R = 0, TEV_G = 1, TEV_B = 2, TEV_A = 3 };

typedef struct {
    s16 rabg[4];
} TevColor;

static TevColor tev_reg_start[4];
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

static RGBA _get_grid_color(const PixelAttributes *p, ColorType t)
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
        const u32 reg = get_bp_register_pointer()[0xF6 + 2 * swap_index];
        for (int j = 0; j < 2; j++) {
            const u8 source_channel = (reg >> (2 * j)) & 0x3;

            ((u8 *)&out)[(i * 2) + j] = ((u8 *)&source)[source_channel];
        }
    }
    return out;
}

static const u8 konst_fractions[8] = {255, 223, 191, 159, 128, 96, 64, 32};
static const u8 rabg_idx[4] = {0, 3, 2, 1};
static s16 _konst_channel(const u8 n)
{
    return tev_konst[n & 3].rabg[rabg_idx[(n - 16) >> 2]];
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
        *r = tev_konst[kc - 12].rabg[0];
        *g = tev_konst[kc - 12].rabg[3];
        *b = tev_konst[kc - 12].rabg[2];
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

void draw_pixel(CPU *cpu, const PixelAttributes *p, u32 x, u32 y, u32 ox, u32 oy)
{
    const u32 *bp = get_bp_register_pointer();
    const u32 gen_mode = bp[0];
    const u32 num_of_steps = (gen_mode & 0xF) + 1;

    for (int i = 0; i < num_of_steps; i++) {
        const u32 tev_order = bp[0x28 + i];
        const bool is_even = i % 2 == 0;

        if (((tev_order >> TEV_ORDER_ENABLE_TEX(is_even)) & 1) == 0) {
            // tex nor enabled
            // assert(!"requires indirect sampling\n");
        }

        const u8 swap_index_grid_color = bp[0xC0 + (i * 2)] & 0x3;
        RGBA grid_color = _get_grid_color(p, (tev_order >> TEV_COLOR_CHANNEL(is_even)) & 0x7);
        grid_color = swap_color(swap_index_grid_color, grid_color);

        const u8 swap_index_texture_color = bp[0xC0 + (i * 2)] & 0x3;
        u8 tex_unit = (tev_order >> TEV_ORDER_TEX_MAP(is_even)) & 0x7;
        u8 tex_cords = (tev_order >> TEV_TEX_CORD(is_even)) & 0x7;

        TextureUnit u;
        get_texture_unit_regs(&u, tex_unit, bp);

        RGBA tex_color = sample_texture(cpu, u, p->tex[tex_cords], p->lod[tex_unit]);
        tex_color = swap_color(swap_index_texture_color, tex_color);

        // constants
        const u8 color_constant = (bp[0xF6 + (i / 2)] >> (i % 2 == 0 ? 4 : 14)) & 0x1F;
        const u8 alpha_constant = (bp[0xF6 + (i / 2)] >> (i % 2 == 0 ? 9 : 19)) & 0x1F;

        s16 r, g, b, a;
        _get_color_constant(color_constant, &r, &g, &b, alpha_constant, &a);
    }
}
