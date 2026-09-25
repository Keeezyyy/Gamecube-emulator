#include "pipeline.h"
#include "core/config/config.h"
#include "graphics/gpu/render/backend/software/clipper/clipper.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/vertex/primitive.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <assert.h>
#include <stdbool.h>

#define SOFTWARE_CLIPPER
#define SOFTWARE_RATERIZER

void load_vertex_into_pipeline(CPU *cpu, Primitive p)
{

    // TODO: find better alternative maybe gloabl dynamic array
    XFOutput xf_output_buffer[U16_MAX] = {0};
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

            if (clipping(&v[i], &v[i + 1], &v[i + 2], xf_regs, clipper_output1, &polygon_count) &&
                clipping(&v[i], &v[i + 2], &v[i + 3], xf_regs, clipper_output2, &polygon_count)) {
                // draw both
            }
        }
        break;

    case GX_TRIANGLES:
        for (int i = 0; i + 2 < n; i += 3) {
            XFOutput clipper_output[MAX_CLIP_VERTS];
            if (clipping(&v[i], &v[i + 1], &v[i + 2], xf_regs, clipper_output, &polygon_count)) {
                //
            }
        }

        break;

    case GX_TRIANGLESTRIP:
        for (int i = 0; i + 2 < n; i++) {

            XFOutput clipper_output[MAX_CLIP_VERTS];
            if (i % 2 == 0) {
                if (clipping(&v[i], &v[i + 1], &v[i + 2], xf_regs, clipper_output,
                             &polygon_count)) {
                }
            } else {

                if (clipping(&v[i + 1], &v[i], &v[i + 2], xf_regs, clipper_output,
                             &polygon_count)) {
                }
            }
        }
        break;

    case GX_TRIANGLEFAN:
        for (int i = 1; i + 1 < n; i++) {
            XFOutput clipper_output[MAX_CLIP_VERTS];
            if (clipping(&v[0], &v[i], &v[i + 1], xf_regs, clipper_output, &polygon_count)) {
            }
        }
        break;

    case GX_LINES:
        for (int i = 0; i + 1 < n; i += 2) {
            XFOutput line_buffer[2];
            if (clip_line(&v[i], &v[i + 1], xf_regs, &line_buffer[0], &line_buffer[1])) {
            }
        }

        break;

    case GX_LINESTRIP:
        for (int i = 0; i + 1 < n; i++) {
            XFOutput line_buffer[2];
            if (clip_line(&v[i], &v[i + 1], xf_regs, &line_buffer[0], &line_buffer[1])) {
            }
        }

        break;

    case GX_POINTS:
        for (int i = 0; i < n; i++)
            if (clip_dot(&v[i], xf_regs)) {
            }

        break;

    default:
        assert(!"unknown primitive type");
    }
#endif

#ifdef SOFTWARE_RATERIZER

#endif
}
