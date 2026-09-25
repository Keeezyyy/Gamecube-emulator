#include "rasterize.h"
#include "bus/bus.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include "../framebuffer.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

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

static void interpolate_s32(s32 ax, s32 by, s32 bx, s32 ay, s32 f1, s32 f2, s32 f3, s32 *dfdx,
                            s32 *dfdy)
{
    s32 A = ax * by - bx * ay;

    s32 F2 = f2 - f1;
    s32 F3 = f3 - f1;

    *dfdx = (F2 * (s32)by - F3 * (s32)ay) / (s32)A;
    *dfdy = (F3 * (s32)ax - F2 * (s32)bx) / (s32)A;
}

static bool test_if_z_test_fails(CPU *cpu, u32 z, s32 x, s32 y)
{
    const u32 ctrl = get_bp_register_pointer()[0x40];
    if ((ctrl & 1) == 0 || (((ctrl >> 1) & 0xF) == 7)) {
        return false;
    }
    u32 z_in_fb = get_z_in_fb(cpu, x, y);
}

static void DrawPixel(CPU *cpu, s32 x, s32 y, XFOutput v[3], Vertex *vert, s32 ax, s32 by, s32 bx,
                      s32 ay)
{
    static f32 dfdx, dfdy, z0, x0, y0;
    if (!((bp(0x00) >> 19) & 1)) {
        interpolate_f32(ax, by, bx, ay, v[0].pos.m[2], v[1].pos.m[2], v[2].pos.m[2], &dfdx, &dfdy);
        dfdx *= 16.0f;
        dfdy *= 16.0f;
        z0 = v[0].pos.m[2];
        x0 = v[0].pos.m[0];
        y0 = v[0].pos.m[1];
    }

    f32 zInterpolated =
        fminf(fmaxf(z0 + dfdx * ((f32)x - x0) + dfdy * ((f32)y - y0), 0.0f), 16777215.0f);

    u32 zDec = (u32)zInterpolated;

    if (test_if_z_test_fails(cpu, zDec, x, y))
        return;
}

void rasterize_polygon(CPU *cpu, const XFOutput in[3], Vertex *vert)
{

    s32 ox, oy;
    get_scissor_offset(&ox, &oy);
    ScissorRect sc = get_scissor(ox, oy, 640, efb_height());

    if (sc.empty)
        return;

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

    for (int y = miny; y < maxy; y++) {
        for (int x = minx; x < maxx; x++) {
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

            DrawPixel(cpu, x, y, v, vert, X[1] - X[0], Y[2] - Y[0], X[2] - X[0], Y[1] - Y[0]);
        }
    }
}
