#include "rasterize.h"
#include "bus/bus.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/tev/tev.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/render/texture/texture.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include "../framebuffer.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <raylib.h>

typedef struct {
    s32 x0, y0, x1, y1;
    bool empty;
} ScissorRect;

static inline u32 bp(u32 reg)
{
    return get_bp_register_pointer()[reg] & 0xFFFFFF;
}

static u32 num_texgens(void)
{
    u32 n = bp(0x00) & 0xF;
    return n > 8 ? 8 : n;
}

static void scale_texcoords(XFOutput *v)
{
    for (u32 i = 0; i < num_texgens(); i++) {
        v->tex[i].m[0] *= (f32)((bp(0x30 + 2 * i) & 0xFFFF) + 1);
        v->tex[i].m[1] *= (f32)((bp(0x31 + 2 * i) & 0xFFFF) + 1);
    }
    // TODO: Bit 16 range_bias, Bit 17 cylindric_wrap
    // TODO: point and line texture offsets BP[0x22]
}

static void get_scissor_offset(s32 *ox, s32 *oy)
{
    u32 so = bp(0x59);
    *ox = (s32)(so & 0x3FF) * 2;
    *oy = (s32)((so >> 10) & 0x3FF) * 2;
}

static s32 wrap_efb(s32 screen, s32 offset)
{
    return ((screen - offset + 342) & 1023) - 342;
}

typedef struct {
    f32 x;
    f32 y;
    f32 z;

} Vec;

static ScissorRect get_scissor(s32 ox, s32 oy, s32 efb_w, s32 efb_h)
{
    u32 tl = bp(0x20), br = bp(0x21);
    ScissorRect r;
    r.x0 = wrap_efb((tl >> 12) & 0x3FF, ox);
    r.y0 = wrap_efb(tl & 0x3FF, oy);
    r.x1 = wrap_efb((br >> 12) & 0x3FF, ox);
    r.y1 = wrap_efb(br & 0x3FF, oy);
    if (r.x0 < 0)
        r.x0 = 0;
    if (r.y0 < 0)
        r.y0 = 0;
    if (r.x1 > efb_w - 1)
        r.x1 = efb_w - 1;
    if (r.y1 > efb_h - 1)
        r.y1 = efb_h - 1;
    r.empty = (r.x0 > r.x1) || (r.y0 > r.y1);
    return r;
}

static s32 efb_height(void)
{
    u32 pixel_format = bp(0x43) & 0x7;
    return (pixel_format == 2) ? 264 : 528;
}

static s32 min3(s32 a, s32 b, s32 c)
{
    if (a > b) {
        if (b > c) {
            return c;
        } else {
            return b;
        }
    } else {
        if (a > c) {
            return c;
        } else {
            return a;
        }
    }
}
static s32 max3(s32 a, s32 b, s32 c)
{
    if (a < b) {
        if (b > c) {
            return b;
        } else {
            return c;
        }
    } else {
        if (a > c) {
            return a;
        } else {
            return c;
        }
    }
}

/*
* Das ist das Werkzeug für alles Weitere (Z, Farben, Texturkoordinaten). Einmal pro Dreieck und
Attribut rechnest du:

  Kantenvektoren in Pixeln (float, nach Abzug des Scissor-Offsets):
  ax = x2 − x1, ay = y2 − y1, bx = x3 − x1, by = y3 − y1
  Doppelte Fläche: A = ax·by − bx·ay
  Wertedifferenzen: F2 = f2 − f1, F3 = f3 − f1
  Steigung in x: dfdx = (F2·by − F3·ay) / A
  Steigung in y: dfdy = (F3·ax − F2·bx) / A

  Der Wert an einem beliebigen Punkt (sx, sy) ist dann:

  f = f1 + dfdx · (sx − x1) + dfdy · (sy − y1)

                                          */
static void interpolate_f32(s32 ax, s32 by, s32 bx, s32 ay, f32 f1, f32 f2, f32 f3, f32 *dfdx,
                            f32 *dfdy)
{
    s32 A = ax * by - bx * ay;

    f32 F2 = f2 - f1;
    f32 F3 = f3 - f1;

    *dfdx = (F2 * (f32)by - F3 * (f32)ay) / (f32)A;
    *dfdy = (F3 * (f32)ax - F2 * (f32)bx) / (f32)A;
}

typedef struct {
    f32 f0;
    f32 dfdx;
    f32 dfdy;
    f32 x0;
    f32 y0;
} Slope;

static Slope z_slope;

static Slope make_slope(const XFOutput v[3], s32 ax, s32 by, s32 bx, s32 ay, f32 f0, f32 f1,
                        f32 f2)
{
    Slope s = {f0, 0.0f, 0.0f, v[0].pos.m[0], v[0].pos.m[1]};
    interpolate_f32(ax, by, bx, ay, f0, f1, f2, &s.dfdx, &s.dfdy);
    s.dfdx *= 16.0f;
    s.dfdy *= 16.0f;
    return s;
}

static f32 slope_at(const Slope *s, f32 x, f32 y)
{
    return s->f0 + s->dfdx * (x - s->x0) + s->dfdy * (y - s->y0);
}

void rasterize_update_zslope(const XFOutput in[3])
{
    if ((bp(0x00) >> 19) & 1)
        return;

    s32 ox, oy;
    get_scissor_offset(&ox, &oy);

    XFOutput v[3];
    s32 X[3], Y[3];
    for (int i = 0; i < 3; i++) {
        v[i] = in[i];
        v[i].pos.m[0] -= (f32)ox;
        v[i].pos.m[1] -= (f32)oy;
        X[i] = (s32)lroundf(v[i].pos.m[0] * 16.0f) - 9;
        Y[i] = (s32)lroundf(v[i].pos.m[1] * 16.0f) - 9;
    }

    if ((X[1] - X[0]) * (Y[2] - Y[0]) - (Y[1] - Y[0]) * (X[2] - X[0]) == 0)
        return;

    z_slope = make_slope(v, X[1] - X[0], Y[2] - Y[0], X[2] - X[0], Y[1] - Y[0], v[0].pos.m[2],
                         v[1].pos.m[2], v[2].pos.m[2]);
}

bool test_if_z_test_fails(CPU *cpu, u32 z, s32 x, s32 y)
{
    const u32 ctrl = get_bp_register_pointer()[0x40];

    if ((ctrl & 1) == 0)
        return false;

    const u32 func = (ctrl >> 1) & 0x7;
    const u32 z_in_fb = get_z_in_fb(cpu, x, y);

    switch (func) {
    case 0:
        return true;
    case 1:
        return z >= z_in_fb;
    case 2:
        return z != z_in_fb;
    case 3:
        return z > z_in_fb;
    case 4:
        return z <= z_in_fb;
    case 5:
        return z == z_in_fb;
    case 6:
        return z < z_in_fb;
    case 7:
        return false;
    }

    return false;
}

static void st_at(const Slope s[3], f32 x, f32 y, s32 *st)
{
    f32 q = slope_at(&s[2], x, y);
    if (q == 0.0f)
        q = 1.0f;

    st[0] = (s32)(slope_at(&s[0], x, y) / q * 128.0f);
    st[1] = (s32)(slope_at(&s[1], x, y) / q * 128.0f);
}

static bool interpolate_pixel(CPU *cpu, s32 x, s32 y, XFOutput v[3], Vertex *vert,
                              Slope color_slopes[2][4], Slope tex_slopes[8][3],
                              PixelAttributes *out)
{
    f32 zInterpolated = fminf(fmaxf(slope_at(&z_slope, (f32)x, (f32)y), 0.0f), 16777215.0f);

    u32 zDec = (u32)zInterpolated;

    if (((bp(0x43) >> 6) & 1) && test_if_z_test_fails(cpu, zDec, x, y))
        return false;

    out->x = x;
    out->y = y;
    out->z = zDec;

    out->edges = v;
    out->vert = vert;

    for (int c = 0; c < 2; c++)
        for (int k = 0; k < 4; k++)
            out->colors[c][k] =
                (u8)fminf(fmaxf(slope_at(&color_slopes[c][k], (f32)x, (f32)y), 0.0f), 255.0f);

    for (u32 i = 0; i < num_texgens(); i++) {
        s32 right[2], down[2];
        st_at(tex_slopes[i], (f32)x, (f32)y, out->tex[i]);
        st_at(tex_slopes[i], (f32)(x + 1), (f32)y, right);
        st_at(tex_slopes[i], (f32)x, (f32)(y + 1), down);

        const s32 ds = max3(abs(right[0] - out->tex[i][0]), abs(down[0] - out->tex[i][0]), 0);
        const s32 dt = max3(abs(right[1] - out->tex[i][1]), abs(down[1] - out->tex[i][1]), 0);

        out->lod[i] = log2f((f32)(ds > dt ? ds : dt) / 128.0f);
    }

    return true;
}

void rasterize_polygon(CPU *cpu, const XFOutput in[3], Vertex *vert)
{

    s32 ox, oy;
    get_scissor_offset(&ox, &oy);
    ScissorRect sc = get_scissor(ox, oy, 640, efb_height());

    if (sc.empty)
        return;

    rasterize_update_zslope(in);

    XFOutput v[3];
    s32 X[3], Y[3];
    for (int i = 0; i < 3; i++) {
        v[i] = in[i];
        scale_texcoords(&v[i]);
        v[i].pos.m[0] -= (f32)ox;
        v[i].pos.m[1] -= (f32)oy;
        X[i] = (s32)lroundf(v[i].pos.m[0] * 16.0f) - 9;
        Y[i] = (s32)lroundf(v[i].pos.m[1] * 16.0f) - 9;
    }

    s32 minx = (min3(X[0], X[1], X[2]) + 0xF) >> 4;
    s32 maxx = (max3(X[0], X[1], X[2]) + 0xF) >> 4;
    s32 miny = (min3(Y[0], Y[1], Y[2]) + 0xF) >> 4;
    s32 maxy = (max3(Y[0], Y[1], Y[2]) + 0xF) >> 4;

    if (minx < sc.x0)
        minx = sc.x0;
    if (miny < sc.y0)
        miny = sc.y0;
    if (maxx > sc.x1 + 1)
        maxx = sc.x1 + 1;
    if (maxy > sc.y1 + 1)
        maxy = sc.y1 + 1;
    if (minx >= maxx || miny >= maxy)
        return;

    minx &= ~1;
    miny &= ~1;
    maxx = (maxx + 1) & ~1;
    maxy = (maxy + 1) & ~1;

    s32 area2 = (X[1] - X[0]) * (Y[2] - Y[0]) - (Y[1] - Y[0]) * (X[2] - X[0]);

    if (area2 == 0)
        return;

    // switch vertex 2 and 3
    if (area2 < 0) {
        s32 x3 = X[1];
        s32 y3 = Y[1];

        X[1] = X[2];
        Y[1] = Y[2];

        X[2] = x3;
        Y[2] = y3;

        XFOutput temp = v[1];
        v[1] = v[2];
        v[2] = temp;
    }
    static const u8 idxs[] = {0, 1, 2, 0};

    s64 dX[3], dY[3], bias[3];
    for (int i = 0; i < 3; i++) {
        int a = idxs[i], b = idxs[i + 1];
        dX[i] = X[b] - X[a];
        dY[i] = Y[b] - Y[a];
        bool topLeft = (dY[i] < 0) || (dY[i] == 0 && dX[i] > 0);
        bias[i] = topLeft ? 1 : 0;
    }

    const u32 gen_mode = get_bp_register_pointer()[0];

    const s32 ax = X[1] - X[0], ay = Y[1] - Y[0], bx = X[2] - X[0], by = Y[2] - Y[0];

    Slope color_slopes[2][4];
    for (int c = 0; c < 2; c++)
        for (int k = 0; k < 4; k++)
            color_slopes[c][k] = make_slope(v, ax, by, bx, ay, ((const u8 *)&v[0].colors[c])[k],
                                            ((const u8 *)&v[1].colors[c])[k],
                                            ((const u8 *)&v[2].colors[c])[k]);

    Slope tex_slopes[8][3];
    for (u32 i = 0; i < num_texgens(); i++)
        for (int k = 0; k < 3; k++)
            tex_slopes[i][k] = make_slope(v, ax, by, bx, ay, v[0].tex[i].m[k] * v[0].pos.m[3],
                                          v[1].tex[i].m[k] * v[1].pos.m[3],
                                          v[2].tex[i].m[k] * v[2].pos.m[3]);

    for (int y = miny; y < maxy; y++) {
        for (int x = minx; x < maxx; x++) {
            if (x < sc.x0 || x > sc.x1 || y < sc.y0 || y > sc.y1)
                continue;

            s64 Px = (s64)x << 4;
            s64 Py = (s64)y << 4;

            bool isIn = true;
            for (int i = 0; i < 3; i++) {
                int a = idxs[i];
                s64 E = dX[i] * (Py - Y[a]) - dY[i] * (Px - X[a]);
                if (E + bias[i] <= 0) {
                    isIn = false;
                    break;
                }
            }

            if (!isIn)
                continue;

            PixelAttributes p = {0};
            if (interpolate_pixel(cpu, x, y, v, vert, color_slopes, tex_slopes, &p)) {
                // passed z test

                // DrawPixel(x + ox - 342, y + oy - 342, YELLOW);

                for (int i = 0; i < ((gen_mode >> 16) & 0x7); i++) {
                    assert(!"indirect used");
                    u8 tex_unit = (get_bp_register_pointer()[0x27] >> (i * 6)) & 0x7;
                    u8 tex_cord = (get_bp_register_pointer()[0x27] >> (i * 6 + 3)) & 0x7;

                    if (tex_cord >= (gen_mode & 0xF))
                        tex_cord = 0;

                    u8 scale_s = (get_bp_register_pointer()[0x25] >> (i * 8)) & 0xF;
                    u8 scale_t = (get_bp_register_pointer()[0x25] >> (i * 8 + 4)) & 0xF;

                    s32 s = (s32)p.tex[tex_cord][0] >> scale_s;
                    s32 t = (s32)p.tex[tex_cord][1] >> scale_t;
                    TextureUnit u;
                    get_texture_unit_regs(&u, tex_unit, get_bp_register_pointer());

                    s32 tex_coords[] = {s, t};

                    RGBA color = sample_texture(cpu, u, tex_coords, p.lod[tex_cord]);
                }

                draw_pixel(cpu, &p, x, y, ox, oy);
            }
        }
    }
}
