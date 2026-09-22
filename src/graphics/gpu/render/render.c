#include "render.h"
#include "core/config/config.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include "utils/vector.h"
#include <stdio.h>
#include <string.h>

static Vector vertex_vector;
static BPRegisters bp;
static CPRegisters cp;
static XFRegisters xf;

static void render_current_state(void)
{
    RENDER_PRINT("[Render] : num of Vertexes : %d\n", vertex_vector.num_of_bytes / sizeof(Vertex));
}

void init_renderer(void)
{
    init_Vector(&vertex_vector, INITIAL_VERTEX_BUFFER_CAP);
}

void push_vertex_to_vertex_buffer(Vertex v)
{
    if ((vertex_vector.cap_in_bytes - vertex_vector.num_of_bytes) < sizeof(Vertex)) {
        vertex_vector.grow(&vertex_vector);
    }

    memcpy(((u8 *)vertex_vector.buffer) + vertex_vector.num_of_bytes, &v, sizeof(Vertex));
    vertex_vector.num_of_bytes += sizeof(Vertex);
}

void cpy_reg_state(u32 *bp_s, u32 *cp_s, u32 *xf_s)
{
    memcpy(&bp, bp_s, sizeof(bp));
    memcpy(&cp, cp_s, sizeof(cp));
    memcpy(&xf, xf_s, sizeof(xf));

    render_current_state();

    //... render

    vertex_vector.num_of_bytes = 0;
}
