#include "render.h"
#include "core/config/config.h"
#include "graphics/gpu/render/matlib.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include "utils/vector.h"

// glad muss vor GLFW kommen, sonst zieht GLFW die System-gl.h zuerst rein.
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <_abort.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static Vector vertex_vector;
static Vector gpu_vertex_vector;

static BPRegisters bp;
static CPRegisters cp;
static XFRegisters xf;

static GLFWwindow *window;

static u8 _get_vertices_count_for_primitive_type(PrimitiveType t)
{
    switch (t) {
    case GX_QUADS:
    case GX_QUADS_2:
        return 4;

    case GX_TRIANGLES:
        return 3;

    case GX_TRIANGLESTRIP:
    case GX_TRIANGLEFAN:
        return 3;

    case GX_LINES:
        return 2;

    case GX_LINESTRIP:
        return 2;

    case GX_POINTS:
        return 1;

    default:
        assert(!"type unknown \n");
    }
}

static GpuVertex prepare_vertex(Vertex *v)
{
    GpuVertex out_v = {0};
    for (int i = 0; i < (2 + (u8)v->pos.position_elements); i++) {
        switch (v->pos.position_data_type) {
        case DATA_TYPE_U8: {
            u8 val = ((u8 *)&v->pos.vec)[i];
            ((f32 *)&out_v.pos.pos)[i] = (f32)val;
            break;
        }
        case DATA_TYPE_S8: {
            s8 val = ((s8 *)&v->pos.vec)[i];
            ((f32 *)&out_v.pos.pos)[i] = (f32)val;
            break;
        }
        case DATA_TYPE_U16: {
            u16 val = ((u16 *)&v->pos.vec)[i];
            ((f32 *)&out_v.pos.pos)[i] = (f32)val;
            break;
        }
        case DATA_TYPE_S16: {
            s16 val = ((s16 *)&v->pos.vec)[i];
            ((f32 *)&out_v.pos.pos)[i] = (f32)val;
            break;
        }
        case DATA_TYPE_F32: {
            f32 val = ((f32 *)&v->pos.vec)[i];
            ((f32 *)&out_v.pos.pos)[i] = val;
            break;
        }
        }
    }

    out_v.pos.has_pos_mat_idx = v->pm.has_pos_mat_idx;
    out_v.pos.pos_mat_idx = v->pm.pos_mat_idx;

    return out_v;
}

static void prepare_vertex_buffer_for_frame(void)
{
    for (int i = 0; i < (gpu_vertex_vector.num_of_bytes / sizeof(GpuVertex)); i++) {
        GpuVertex *g = &((GpuVertex *)gpu_vertex_vector.buffer)[i];

        if (!g->pos.has_pos_mat_idx) {
            g->pos.has_pos_mat_idx = true;
            g->pos.pos_mat_idx = cp.cp_registers[0x30] & 0b111111;
        }
    }
}

static void swap_buffers(void)
{
    glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glfwSwapBuffers(window);
    glfwPollEvents();

    if (glfwWindowShouldClose(window)) {
        printf("[Render] : window was closed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        abort();
    }
}

static void render_current_state(void)
{
    RENDER_PRINT("[Render] : num of Vertexes : %d\n", vertex_vector.num_of_bytes / sizeof(Vertex));

    prepare_vertex_buffer_for_frame();

    //
    swap_buffers();
}

void init_renderer(void)
{

    init_Vector(&vertex_vector, INITIAL_VERTEX_BUFFER_CAP);
    init_Vector(&gpu_vertex_vector, INITIAL_VERTEX_BUFFER_CAP);

    if (!glfwInit()) {
        printf("[ERRPR]: glfw init failed\n");
        abort();
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(640, 480, "gc emu", NULL_PTR, NULL_PTR);

    glfwMakeContextCurrent(window);

    if (!gladLoadGL(glfwGetProcAddress)) {
        ERROR("glad load gl");
        glfwDestroyWindow(window);
        glfwTerminate();
        abort();
    }
}

void push_vertex_to_vertex_buffer(Vertex v)
{
    if ((vertex_vector.cap_in_bytes - vertex_vector.num_of_bytes) < sizeof(Vertex)) {
        vertex_vector.grow(&vertex_vector);
    }

    memcpy(((u8 *)vertex_vector.buffer) + vertex_vector.num_of_bytes, &v, sizeof(Vertex));
    vertex_vector.num_of_bytes += sizeof(Vertex);

    GpuVertex g_v = prepare_vertex(&v);

    if ((gpu_vertex_vector.cap_in_bytes - gpu_vertex_vector.num_of_bytes) < sizeof(GpuVertex)) {
        gpu_vertex_vector.grow(&gpu_vertex_vector);
    }
    memcpy(((u8 *)gpu_vertex_vector.buffer) + gpu_vertex_vector.num_of_bytes, &g_v,
           sizeof(GpuVertex));
    gpu_vertex_vector.num_of_bytes += sizeof(GpuVertex);
}

void cpy_reg_state(u32 *bp_s, u32 *cp_s, u32 *xf_s)
{
    memcpy(&bp, bp_s, sizeof(bp));
    memcpy(&cp, cp_s, sizeof(cp));
    memcpy(&xf, xf_s, sizeof(xf));

    render_current_state();

    //... render

    vertex_vector.num_of_bytes = 0;
    gpu_vertex_vector.num_of_bytes = 0;
}
