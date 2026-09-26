#include "pipeline.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/clipper/clipper.h"
#include "graphics/gpu/render/backend/software/rasterize/rasterize.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/vertex/primitive.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include "utils/vector.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#ifdef RENDER_TEST_RAYLIB
#include <raylib.h>
#include <rlgl.h>
#endif

#define SOFTWARE_CLIPPER
#define SOFTWARE_RATERIZER

void init_raylib_renderer_test(void)
{
#ifdef RENDER_TEST_RAYLIB
    const int screenWidth = 640;
    const int screenHeight = 480;

    InitWindow(screenWidth, screenHeight, "render test");
    rlDisableBackfaceCulling();
    BeginDrawing();
    ClearBackground(BLACK);
#endif
}

static void draw_polygon(CPU *cpu, const XFOutput *out, const u16 polygon_count, Vertex *v)
{

    XFOutput polygon[3];
    for (int i = 0; i < polygon_count; i++) {
        polygon[0] = out[0];
        polygon[1] = out[i + 1];
        polygon[2] = out[i + 2];
        rasterize_polygon(cpu, polygon, v);
    }
}

static void draw_quad(CPU *cpu, const XFOutput q[4], Vertex *v)
{
    const XFOutput t0[3] = {q[0], q[1], q[2]};
    const XFOutput t1[3] = {q[0], q[2], q[3]};
    rasterize_polygon(cpu, t0, v);
    rasterize_polygon(cpu, t1, v);
}

static void draw_line(CPU *cpu, const XFOutput *a, const XFOutput *b, Vertex *v)
{
    const f32 w = (f32)(get_bp_register_pointer()[0x22] & 0xFF) / 12.0f;
    const int axis =
        fabsf(b->pos.m[0] - a->pos.m[0]) >= fabsf(b->pos.m[1] - a->pos.m[1]) ? 1 : 0;

    XFOutput q[4] = {*a, *b, *b, *a};
    q[0].pos.m[axis] -= w;
    q[1].pos.m[axis] -= w;
    q[2].pos.m[axis] += w;
    q[3].pos.m[axis] += w;
    draw_quad(cpu, q, v);
}

static void draw_point(CPU *cpu, const XFOutput *pt, Vertex *v)
{
    const f32 s = (f32)((get_bp_register_pointer()[0x22] >> 8) & 0xFF) / 12.0f;

    XFOutput q[4] = {*pt, *pt, *pt, *pt};
    q[0].pos.m[0] -= s;
    q[0].pos.m[1] -= s;
    q[1].pos.m[0] += s;
    q[1].pos.m[1] -= s;
    q[2].pos.m[0] += s;
    q[2].pos.m[1] += s;
    q[3].pos.m[0] -= s;
    q[3].pos.m[1] += s;
    draw_quad(cpu, q, v);
}

void load_vertex_into_pipeline(CPU *cpu, Primitive p)
{

    // TODO: find better alternative maybe gloabl dynamic array
    static Vector xf_output_vector;
    if (!xf_output_vector.buffer)
        init_Vector(&xf_output_vector, sizeof(XFOutput) * 1024);

    while (xf_output_vector.cap_in_bytes < p.vert_count * sizeof(XFOutput))
        xf_output_vector.grow(&xf_output_vector);

    XFOutput *xf_output_buffer = xf_output_vector.buffer;
    const XF_Registers *xf_regs;

    for (int i = 0; i < p.vert_count; i++) {
        // XF TRANSFORM
#ifdef SOFTWARE_TRANSFORM
        xf_output_buffer[i] = transform_vertex(cpu, &p.vertecies[i]);
        xf_regs = get_xf_regs();
#endif
    }

    u16 polygon_count = 0;
    bool should_draw = true;

#ifdef SOFTWARE_CLIPPER
    const int n = p.vert_count;
    XFOutput *v = xf_output_buffer;

    switch (p.t) {
    case GX_QUADS:
    case GX_QUADS_2:
        for (int i = 0; i + 3 < n; i += 4) {

            XFOutput clipper_output1[MAX_CLIP_VERTS];
            XFOutput clipper_output2[MAX_CLIP_VERTS];

            if (clipping(&v[i], &v[i + 1], &v[i + 2], xf_regs, clipper_output1, &polygon_count)) {
                draw_polygon(cpu, clipper_output1, polygon_count, p.vertecies);
            }
            if (clipping(&v[i], &v[i + 2], &v[i + 3], xf_regs, clipper_output2, &polygon_count)) {
                draw_polygon(cpu, clipper_output2, polygon_count, p.vertecies);
            }
        }
        break;

    case GX_TRIANGLES:
        for (int i = 0; i + 2 < n; i += 3) {
            XFOutput clipper_output[MAX_CLIP_VERTS];
            if (clipping(&v[i], &v[i + 1], &v[i + 2], xf_regs, clipper_output, &polygon_count)) {
                draw_polygon(cpu, clipper_output, polygon_count, p.vertecies);
            }
        }

        break;

    case GX_TRIANGLESTRIP:
        for (int i = 0; i + 2 < n; i++) {

            XFOutput clipper_output[MAX_CLIP_VERTS];
            if (i % 2 == 0) {
                if (clipping(&v[i], &v[i + 1], &v[i + 2], xf_regs, clipper_output,
                             &polygon_count)) {
                    draw_polygon(cpu, clipper_output, polygon_count, p.vertecies);
                }
            } else {

                if (clipping(&v[i + 1], &v[i], &v[i + 2], xf_regs, clipper_output,
                             &polygon_count)) {
                    draw_polygon(cpu, clipper_output, polygon_count, p.vertecies);
                }
            }
        }
        break;

    case GX_TRIANGLEFAN:
        for (int i = 1; i + 1 < n; i++) {
            XFOutput clipper_output[MAX_CLIP_VERTS];
            if (clipping(&v[0], &v[i], &v[i + 1], xf_regs, clipper_output, &polygon_count)) {
                draw_polygon(cpu, clipper_output, polygon_count, p.vertecies);
            }
        }
        break;

    case GX_LINES:
        for (int i = 0; i + 1 < n; i += 2) {
            XFOutput line_buffer[2];
            if (clip_line(&v[i], &v[i + 1], xf_regs, &line_buffer[0], &line_buffer[1])) {
                draw_line(cpu, &line_buffer[0], &line_buffer[1], p.vertecies);
            }
        }

        break;

    case GX_LINESTRIP:
        for (int i = 0; i + 1 < n; i++) {
            XFOutput line_buffer[2];
            if (clip_line(&v[i], &v[i + 1], xf_regs, &line_buffer[0], &line_buffer[1])) {
                draw_line(cpu, &line_buffer[0], &line_buffer[1], p.vertecies);
            }
        }

        break;

    case GX_POINTS:
        for (int i = 0; i < n; i++) {
            XFOutput point = v[i];
            if (clip_dot(&point, xf_regs)) {
                draw_point(cpu, &point, p.vertecies);
            }
        }

        break;

    default:
        assert(!"unknown primitive type");
    }
#endif

#ifdef SOFTWARE_RATERIZER

#endif
}
