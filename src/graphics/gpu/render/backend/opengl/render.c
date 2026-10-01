#include "render.h"
#include "core/config/config.h"
#include "graphics/gpu/render/backend/opengl/registers.h"
#include "graphics/gpu/render/backend/opengl/shader/utils/shader.h"
#include "graphics/gpu/render/backend/opengl/vertex_conversion.h"
#include "graphics/gpu/render/xf_types.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <_abort.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stddef.h>

static GLFWwindow *window;
static GLuint frag_shader;
static GLuint vert_shader;
static GLuint shader_program;

static GLuint VAO;
static GLuint vertex_buffer;
static GLint viewport_loc;
GLint projection_loc;

#define VERTEX_BUFFER_SIZE (U16_MAX * sizeof(Vertex))
static GLint vertex_offset;
void opengl_render_primitive(CPU *cpu, Primitive *p)
{
    if (is_tex_unit_dirty())
        reupload_texture_units(cpu);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

    if ((size_t)(vertex_offset + p->vert_count) * sizeof(GpuVertex) > VERTEX_BUFFER_SIZE) {
        glBufferData(GL_ARRAY_BUFFER, VERTEX_BUFFER_SIZE, NULL_PTR, GL_DYNAMIC_DRAW);
        vertex_offset = 0;
    }

    GpuVertex *dst = glMapBufferRange(
        GL_ARRAY_BUFFER, (GLintptr)((size_t)vertex_offset * sizeof(GpuVertex)),
        (GLsizeiptr)(p->vert_count * sizeof(GpuVertex)),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

    for (int i = 0; i < p->vert_count; i++) {
        dst[i] = vertex_to_gpu(&p->vertecies[i]);
    }

    glUnmapBuffer(GL_ARRAY_BUFFER);

    switch (p->t) {
    case GX_QUADS:
    case GX_QUADS_2:
        for (int i = 0; i < p->vert_count / 4; i++) {
            glDrawArrays(GL_TRIANGLE_FAN, vertex_offset + i * 4, 4);
        }
        break;
    case GX_TRIANGLES:
        glDrawArrays(GL_TRIANGLES, vertex_offset, p->vert_count);
        break;
    case GX_TRIANGLESTRIP:
        glDrawArrays(GL_TRIANGLE_STRIP, vertex_offset, p->vert_count);
        break;
    case GX_TRIANGLEFAN:
        glDrawArrays(GL_TRIANGLE_FAN, vertex_offset, p->vert_count);
        break;
    case GX_LINES:
        glDrawArrays(GL_LINES, vertex_offset, p->vert_count);
        break;
    case GX_LINESTRIP:
        glDrawArrays(GL_LINE_STRIP, vertex_offset, p->vert_count);
        break;
    case GX_POINTS:
        glDrawArrays(GL_POINTS, vertex_offset, p->vert_count);
        break;

    default:
    }

    vertex_offset += p->vert_count;
}

void opengl_finish_frame(void)
{
    glfwSwapBuffers(window);

    if (glfwWindowShouldClose(window)) {
        RENDER_PRINT("[Render] : window was closed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        abort();
    }

    glUseProgram(shader_program);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glfwPollEvents();

    glClear(GL_COLOR_BUFFER_BIT);

    glBufferData(GL_ARRAY_BUFFER, VERTEX_BUFFER_SIZE, NULL_PTR, GL_DYNAMIC_DRAW);
    vertex_offset = 0;
}

void init_opengl_renderer(void)
{
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
    glUseProgram(shader_program);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    //------------------------------------------------------------------------
    // Vertex Buffers
    //------------------------------------------------------------------------
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &vertex_buffer);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, VERTEX_BUFFER_SIZE, NULL_PTR, GL_DYNAMIC_DRAW);

    gpu_vertex_setup_attribs();
    projection_loc = glGetUniformLocation(shader_program, "proj");
    viewport_loc = glGetUniformLocation(shader_program, "view");

    opengl_init_register(shader_program);
}
