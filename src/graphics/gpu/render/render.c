#include "render.h"
#include "core/config/config.h"
#include "graphics/gpu/render/matlib.h"
#include "graphics/gpu/render/shader/shader.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include "utils/vector.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <_abort.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static Vector vertex_vector;
static Vector gpu_vertex_vector;

static BPRegisters bp;
static CPRegisters cp;
static XFRegisters xf;

static GLFWwindow *window;

static u32 frag_shader;
static u32 vert_shader;
static u32 shader_program;

static GLuint VAO;
static GLuint vertex_buffer;
static GLuint matrixBuffer;
static GLint projection_loc;
static GLint viewport_loc;

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
            u8 val = (u8)((u32 *)&v->pos.vec)[i];
            ((f32 *)&out_v.pos.pos)[i] = (f32)val;
            break;
        }
        case DATA_TYPE_S8: {
            s8 val = (s8)((u32 *)&v->pos.vec)[i];
            ((f32 *)&out_v.pos.pos)[i] = (f32)val;
            break;
        }
        case DATA_TYPE_U16: {
            u16 val = (u16)((u32 *)&v->pos.vec)[i];
            ((f32 *)&out_v.pos.pos)[i] = (f32)val;
            break;
        }
        case DATA_TYPE_S16: {
            s16 val = (s16)((u32 *)&v->pos.vec)[i];
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

    if (v->pos.frac != 0 && v->pos.position_data_type != DATA_TYPE_F32) {
        for (int i = 0; i < (2 + (u8)v->pos.position_elements); i++) {
            f32 *cord = &out_v.pos.pos.x;

            cord[i] /= powf(2.0f, (f32)v->pos.frac);
        }
    }

    out_v.pos.has_pos_mat_idx = v->pm.has_pos_mat_idx;
    out_v.pos.pos_mat_idx = v->pm.pos_mat_idx;

    memcpy(&out_v.color.r, v->color->rgba, 4 * sizeof(u8));
    memcpy(out_v.pos_mat, v->pm.mat, sizeof(Float4) * 3);

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

static void _bind_buffers(void)
{

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, gpu_vertex_vector.num_of_bytes, gpu_vertex_vector.buffer);

    glBindBuffer(GL_UNIFORM_BUFFER, matrixBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64 * sizeof(Float4), xf.xf_registers);
}

static Mat4 build_projection(void)
{
    Mat4 out = {0};

    float p0 = ((f32 *)xf.xf_registers)[0x1020];
    float p1 = ((f32 *)xf.xf_registers)[0x1021];
    float p2 = ((f32 *)xf.xf_registers)[0x1022];
    float p3 = ((f32 *)xf.xf_registers)[0x1023];
    float p4 = ((f32 *)xf.xf_registers)[0x1024];
    float p5 = ((f32 *)xf.xf_registers)[0x1025];

    if (xf.xf_registers[0x1026] == 0) {
        // perspektivisch
        out.m[0][0] = p0;
        out.m[0][2] = p1;
        out.m[1][1] = p2;
        out.m[1][2] = p3;
        out.m[2][2] = p4;
        out.m[2][3] = p5;
        out.m[3][2] = -1.0f;
        out.m[3][3] = 0.0f;
    } else {
        // orthografisch
        out.m[0][0] = p0;
        out.m[0][3] = p1;
        out.m[1][1] = p2;
        out.m[1][3] = p3;
        out.m[2][2] = p4;
        out.m[2][3] = p5;
        out.m[3][3] = 1.0f;
    }
    return out;
}
static Mat4 build_viewport(void)
{
    float sx = ((f32 *)xf.xf_registers)[0x101A];
    float sy = ((f32 *)xf.xf_registers)[0x101B];
    float sz = ((f32 *)xf.xf_registers)[0x101C];

    float cx = ((f32 *)xf.xf_registers)[0x101D];
    float cy = ((f32 *)xf.xf_registers)[0x101E];
    float farZ = ((f32 *)xf.xf_registers)[0x101F];

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

static void _draw_primitives(void)
{

    RENDER_PRINT("-----\n");
    for (int i = 0; i < (vertex_vector.num_of_bytes / sizeof(Vertex));) {
        Vertex v = ((Vertex *)vertex_vector.buffer)[i];

        RENDER_PRINT("type : %d, first : %d\n", v.type, i);

        switch (v.type) {
        case GX_QUADS:
        case GX_QUADS_2:
            for (u16 q = 0; q < v.count / 4; q++) {
                glDrawArrays(GL_TRIANGLE_FAN, i + q * 4, 4);
            }
            break;

        case GX_TRIANGLES:
            glDrawArrays(GL_TRIANGLES, i, v.count);
            break;

        case GX_TRIANGLESTRIP:

            glDrawArrays(GL_TRIANGLE_STRIP, i, v.count);
            break;
        case GX_TRIANGLEFAN:

            glDrawArrays(GL_TRIANGLE_FAN, i, v.count);
            break;

        case GX_LINES:
            glDrawArrays(GL_LINES, i, v.count);
            break;

        case GX_LINESTRIP:
            glDrawArrays(GL_LINE_STRIP, i, v.count);
            break;

        case GX_POINTS:
            glDrawArrays(GL_POINTS, i, v.count);
            break;

        default:
            assert(!"type unknown \n");
        }
        i += v.count;
    }

    RENDER_PRINT("-----\n");
}

static void _set_render_settings(void)
{
    u8 line_size = ((u32 *)bp.bp_registers)[0x22] & 0xFF;
    glLineWidth((float)line_size / 6.0f);

    u8 point_size = (((u32 *)bp.bp_registers)[0x22] >> 8) & 0xFF;
    glPointSize((float)point_size / 6.0f);
}

static void swap_buffers(void)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glfwPollEvents();

    glClear(GL_COLOR_BUFFER_BIT);

    _bind_buffers();
    _set_render_settings();

    // load projection

    glUseProgram(shader_program);

    Mat4 proj = build_projection();
    Mat4 view = build_viewport();

    glUniformMatrix4fv(projection_loc, 1, GL_TRUE, &proj.m[0][0]);
    glViewport(0, 0, 640, 480);

    glBindVertexArray(VAO);

    _draw_primitives();

    glfwSwapBuffers(window);

    if (glfwWindowShouldClose(window)) {
        RENDER_PRINT("[Render] : window was closed\n");
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
        RENDER_PRINT("[ERRPR]: glfw init failed\n");
        abort();
    }

    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
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

    // Shader
    //------------------------------------------------------------------------
    vert_shader = glCreateShader(GL_VERTEX_SHADER);
    const char *vert_str = load_shader("./build/shader/vert.glsl");
    glShaderSource(vert_shader, 1, &vert_str, NULL_PTR);
    glCompileShader(vert_shader);
    free_shader(vert_str);

    frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    const char *frag_str = load_shader("./build/shader/frag.glsl");
    glShaderSource(frag_shader, 1, &frag_str, NULL_PTR);
    glCompileShader(frag_shader);
    free_shader(frag_str);

    shader_program = glCreateProgram();
    glAttachShader(shader_program, vert_shader);
    glAttachShader(shader_program, frag_shader);
    glLinkProgram(shader_program);

    /*
      glDeleteShader(vert_shader);
      glDeleteShader(frag_shader);
    */
    //------------------------------------------------------------------------

    // Vertex Buffers
    //------------------------------------------------------------------------
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &vertex_buffer);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, MAX_RENDER_VERTICES * sizeof(GpuVertex), NULL_PTR,
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex),
                          (void *)offsetof(GpuVertex, pos));
    glEnableVertexAttribArray(0);

    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(GpuVertex),
                           (void *)offsetof(GpuPos, is_3d));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GpuVertex),
                          (void *)offsetof(GpuVertex, color));
    glEnableVertexAttribArray(2);

    for (int i = 0; i < 3; i++) {
        glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(GpuVertex),
                              (void *)(offsetof(GpuVertex, pos_mat) + i * sizeof(Float4)));
        glEnableVertexAttribArray(3 + i);
    }

    // matrix buffer
    glGenBuffers(1, &matrixBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, matrixBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Float4) * 64, NULL_PTR, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, matrixBuffer);
    glUniformBlockBinding(shader_program, glGetUniformBlockIndex(shader_program, "Matrices"), 0);

    //------------------------------------------------------------------------
    //
    projection_loc = glGetUniformLocation(shader_program, "proj");
    viewport_loc = glGetUniformLocation(shader_program, "view");
}

void push_vertex_to_vertex_buffer(Vertex v)
{
    if ((vertex_vector.cap_in_bytes - vertex_vector.num_of_bytes) < sizeof(Vertex)) {
        vertex_vector.grow(&vertex_vector);
    }

    memcpy(((u8 *)vertex_vector.buffer) + vertex_vector.num_of_bytes, &v, sizeof(Vertex));
    vertex_vector.num_of_bytes += sizeof(Vertex);

    // printf("the vertices are : %s\n", v.pos.position_elements == POS_ELEMNTS_XY ? "2D" : "3D");

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
