#include "rasterize.h"
#include "graphics/cp/cp.h"
#include <math.h>

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

void rasterize_polygon(const XFOutput in[3])
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
}
