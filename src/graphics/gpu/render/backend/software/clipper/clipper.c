#include "clipper.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include <stdbool.h>
#include <string.h>
#include "core/config/config.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <cblas.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
enum {
    CLIP_POS_X = 1 << 0,
    CLIP_NEG_X = 1 << 1,
    CLIP_POS_Y = 1 << 2,
    CLIP_NEG_Y = 1 << 3,
    CLIP_POS_Z = 1 << 4,
    CLIP_NEG_Z = 1 << 5,
};

static inline u32 calc_clip_mask(const Float4 *v)
{
    const f32 x = v->m[0];
    const f32 y = v->m[1];
    const f32 z = v->m[2];
    const f32 w = v->m[3];

    u32 mask = 0;
    if (w - x < 0.0f)
        mask |= CLIP_POS_X;
    if (w + x < 0.0f)
        mask |= CLIP_NEG_X;
    if (w - y < 0.0f)
        mask |= CLIP_POS_Y;
    if (w + y < 0.0f)
        mask |= CLIP_NEG_Y;
    if (w * z > 0.0f)
        mask |= CLIP_POS_Z;
    if (z + w < 0.0f)
        mask |= CLIP_NEG_Z;
    return mask;
}

typedef struct {
    f32 x;
    f32 y;
    f32 z;
    f32 w;
} Vec4;

static bool test_backface(const Vec4 *v0, const Vec4 *v1, const Vec4 *v2,
                          const XF_Registers *xf_regs)
{
    // n = (x0·w2 − x2·w0)·y1 + (x2·y0 − x0·y2)·w1 + (y2·w0 − y0·w2)·x1

    f32 n = (v0->x * v2->w - v2->x * v0->w) * v1->y + (v2->x * v0->y - v0->x * v2->y) * v1->w +
            (v2->y * v0->w - v0->y * v2->w) * v1->x;

    bool b = n <= 0;

    if ((xf_regs->viewport[1]) > 0) {
        return !b;
    }
    return b;
}

static f32 plane_dist(const Float4 *p, const int plane)
{
    const f32 x = p->m[0];
    const f32 y = p->m[1];
    const f32 z = p->m[2];
    const f32 w = p->m[3];

    switch (plane) {
    case 0:
        return w - x;
    case 1:
        return w + x;
    case 2:
        return w - y;
    case 3:
        return w + y;
    case 4:
        return -z;
    default:
        return z + w;
    }
}

static void lerp_f32(f32 *out, const f32 *a, const f32 *b, const f32 t, const int n)
{
    for (int i = 0; i < n; i++)
        out[i] = a[i] + (b[i] - a[i]) * t;
}

static XFOutput intersection_point(const XFOutput *a, const XFOutput *b, const int plane)
{
    const f32 da = plane_dist(&a->pos, plane);
    const f32 db = plane_dist(&b->pos, plane);
    const f32 t = da / (da - db);

    XFOutput out = *a;

    lerp_f32(out.pos.m, a->pos.m, b->pos.m, t, 4);
    lerp_f32(out.eye.m, a->eye.m, b->eye.m, t, 3);
    lerp_f32(&out.NBT[0].m[0], &a->NBT[0].m[0], &b->NBT[0].m[0], t, 3 * 3);
    lerp_f32(&out.tex[0].m[0], &a->tex[0].m[0], &b->tex[0].m[0], t, 8 * 3);

    for (int c = 0; c < 2; c++) {
        for (int k = 0; k < 4; k++) {
            const f32 ca = (f32)a->colors[c].rgba[k];
            const f32 cb = (f32)b->colors[c].rgba[k];
            out.colors[c].rgba[k] = (u8)(ca + (cb - ca) * t + 0.5f);
        }
    }

    return out;
}

static u32 clip_polygon(XFOutput *poly, u32 count, XFOutput *clipped)
{

    for (int plane = 0; plane < 6 && count > 0; plane++) {
        u32 n = 0;

        for (u32 i = 0; i < count; i++) {
            const XFOutput *p0 = &poly[i];
            const XFOutput *p1 = &poly[(i + 1) % count];

            const bool in0 = plane_dist(&p0->pos, plane) >= 0.0f;
            const bool in1 = plane_dist(&p1->pos, plane) >= 0.0f;

            if (in0) {
                if (in1) {
                    clipped[n++] = *p1;
                } else {
                    clipped[n++] = intersection_point(p0, p1, plane);
                }
            } else if (in1) {
                clipped[n++] = intersection_point(p0, p1, plane);
                clipped[n++] = *p1;
            }
        }

        memcpy(poly, clipped, sizeof(XFOutput) * n);
        count = n;
    }

    return count;
}
static Mat4 build_viewport(const XF_Registers *xf_regs)
{
    float sx = xf_regs->viewport[0];
    float sy = xf_regs->viewport[1];
    float sz = xf_regs->viewport[2];

    float cx = xf_regs->viewport[3];
    float cy = xf_regs->viewport[4];
    float farZ = xf_regs->viewport[5];
    Mat4 m = {0};

    m.m[0][0] = sx;
    m.m[0][3] = cx;

    m.m[1][1] = sy;
    m.m[1][3] = cy;

    m.m[2][2] = sz;
    m.m[2][3] = farZ;

    m.m[3][3] = 1.0f;

    return m;
}
static inline float clampf(float x, float min, float max)
{
    if (x < min)
        return min;
    if (x > max)
        return max;
    return x;
}

bool clipping(XFOutput *clipping_in1, XFOutput *clipping_in2, XFOutput *clipping_in3,
              const XF_Registers *xf_regs, XFOutput *out, u16 *polygon_count)
{
    const Float4 *v1 = &clipping_in1->pos;
    const Float4 *v2 = &clipping_in2->pos;
    const Float4 *v3 = &clipping_in3->pos;

    u32 cr0 = calc_clip_mask(v1);
    u32 cr1 = calc_clip_mask(v2);
    u32 cr2 = calc_clip_mask(v3);

    if ((cr0 & cr1 & cr2) != 0)
        return false;

    bool backface = test_backface((const Vec4 *)v1, (const Vec4 *)v2, (const Vec4 *)v3, xf_regs);

    u8 cull_mode = (get_bp_register_pointer()[0] >> 14) & 0x3;

    if (cull_mode == 3 || (cull_mode == 1 && backface) || (cull_mode == 2 && !backface)) {
        return false;
    }

    // TODO: genmode bit 19 zfreeze

    XFOutput poly[MAX_CLIP_VERTS] = {*clipping_in1, *clipping_in2, *clipping_in3};
    const u32 count = clip_polygon(poly, 3, out);

    if (count < 3)
        return false;

    Mat4 view = build_viewport(xf_regs);
    for (int i = 0; i < count; i++) {
        const f32 inv_w = 1.0f / out[i].pos.m[3];

        Float4 pos = {0};
        pos.m[0] *= inv_w;
        pos.m[1] *= inv_w;
        pos.m[2] *= inv_w;
        pos.m[2] = 1.0f;

        cblas_sgemv(CblasRowMajor, CblasNoTrans, 4, 4, 1.0f, &view.m[0][0], 4, pos.m, 1, 0.0f,
                    out[i].pos.m, 1);

        out[i].pos.m[2] = clampf(out[i].pos.m[2], 0.0f, 16777215.0f);

        out[i].pos.m[3] = inv_w;
    }

    *polygon_count = count - 2;
}
