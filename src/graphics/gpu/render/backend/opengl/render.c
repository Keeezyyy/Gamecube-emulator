#include "render.h"
#include "core/config/config.h"
#include "graphics/gpu/render/backend/opengl/registers.h"
#include "graphics/gpu/render/backend/opengl/shader/utils/shader.h"
#include "graphics/gpu/render/backend/opengl/vertex_conversion.h"
#include "graphics/gpu/render/backend/software/framebuffer.h"
#include "graphics/gpu/render/xf_types.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <_abort.h>
#include <GLFW/glfw3.h>
#include <stddef.h>
#include <string.h>

static GLFWwindow *window;
static GLuint frag_shader;
static GLuint vert_shader;
static GLuint shader_program;

static GLuint VAO;
static GLuint vertex_buffer;
static GLint viewport_loc;
GLint projection_loc;

static GLuint fbo, colorTex;

#define VERTEX_BUFFER_SIZE (U16_MAX * sizeof(Vertex))
static GLint vertex_offset;
void opengl_render_primitive(CPU *cpu, Primitive *p)
{
    if (is_tex_unit_dirty())
        reupload_texture_units(cpu);
    check_for_xf_reg_dirty();

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

    const u32 zmode = get_bp_register_pointer()[0x40];
    if (zmode & 1)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_NEVER + ((zmode >> 1) & 7));
    glDepthMask((zmode >> 4) & 1);

    static const GLenum blend_src[] = {GL_ZERO,      GL_ONE,
                                       GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
                                       GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                       GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA};
    static const GLenum blend_dst[] = {GL_ZERO,      GL_ONE,
                                       GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
                                       GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                       GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA};
    const u32 cmode = get_bp_register_pointer()[0x41];
    const GLboolean color_update = (cmode >> 3) & 1;
    glColorMask(color_update, color_update, color_update, (cmode >> 4) & 1);
    if (cmode & 1) {
        glDisable(GL_COLOR_LOGIC_OP);
        glEnable(GL_BLEND);
        if ((cmode >> 11) & 1) {
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
            glBlendFunc(GL_ONE, GL_ONE);
        } else {
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(blend_src[(cmode >> 8) & 7], blend_dst[(cmode >> 5) & 7]);
        }
    } else if ((cmode >> 1) & 1) {
        glDisable(GL_BLEND);
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(GL_CLEAR + ((cmode >> 12) & 0xF));
    } else {
        glDisable(GL_BLEND);
        glDisable(GL_COLOR_LOGIC_OP);
    }

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

static RGBA framebuffer[XFB_HEIGHT][XFB_WIDTH];

static void opengl_copy_to_efb(CPU *cpu)
{
    RGBA(*xfb)[XFB_WIDTH] = (RGBA(*)[XFB_WIDTH])cpu->bus->xfb;

    lock_xfb();
    for (u32 y = 0; y < XFB_HEIGHT; y++) {
        memcpy(xfb[y], framebuffer[XFB_HEIGHT - 1 - y], sizeof framebuffer[0]);
        for (u32 x = 0; x < XFB_WIDTH; x++)
            xfb[y][x].a = 0xFF;
    }
    unlock_xfb();
}

void opengl_finish_frame(CPU *cpu)
{
    /*
      if (glfwWindowShouldClose(window)) {
          RENDER_PRINT("[Render] : window was closed\n");
          glfwDestroyWindow(window);
          glfwTerminate();
          abort();
      }
      */

    glUseProgram(shader_program);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glBufferData(GL_ARRAY_BUFFER, VERTEX_BUFFER_SIZE, NULL_PTR, GL_DYNAMIC_DRAW);
    vertex_offset = 0;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, 640, 480, 0, 0, 640, 480, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);

    opengl_copy_to_efb(cpu);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 640, 480);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

GLuint opengl_get_shader_program(void)
{
    return shader_program;
}

void opengl_create_context(void)
{
    if (!glfwInit()) {
        RENDER_PRINT("[ERRPR]: glfw init failed\n");
        abort();
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(640, 480, "gc emu", NULL_PTR, NULL_PTR);
    if (!window) {
        RENDER_PRINT("[ERRPR]: glfw window creation failed\n");
        glfwTerminate();
        abort();
    }
}

void init_opengl_renderer(void)
{
    glfwMakeContextCurrent(window);

    if (!gladLoadGL(glfwGetProcAddress)) {
        ERROR("glad load gl");
        glfwDestroyWindow(window);
        glfwTerminate();
        abort();
    }

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 640, 480);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(stderr, "framebuffer error\n");

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
