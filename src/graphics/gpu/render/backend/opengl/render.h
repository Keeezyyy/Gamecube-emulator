#pragma once

#include "bus/bus.h"
#include "core/config/config.h"
#include "graphics/gpu/vertex/primitive.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>

enum {
    GPUV_FLAG_POS_3D = 1u << 0,
    GPUV_FLAG_NORMAL = 1u << 1,
    GPUV_FLAG_NBT = 1u << 2,
    GPUV_FLAG_COLOR0 = 1u << 3,
    GPUV_FLAG_COLOR1 = 1u << 4,
    GPUV_TEX_SHIFT = 8,
    GPUV_TEXMAT_SHIFT = 16,
    GPUV_POSMAT_SHIFT = 24,
};

typedef union {
    u32 buffer;
    struct {
        u8 pos_mat_idx;
        u8 norm_mat_idx;
        u8 tex1_mat_idx;
        u8 tex2_mat_idx;
    };
} MatIndices;

typedef struct {
    float pos[3];
    float normal[3];
    float binormal[3];
    float tangent[3];
    u8 color[2][4];
    float tex[8][2];
    u32 flags;
    u32 tex_mat_idx[2];
    MatIndices mat_indices;

} GpuVertex;

_Static_assert(offsetof(GpuVertex, color) % 4 == 0, "align");
_Static_assert(sizeof(GpuVertex) % 4 == 0, "align");

void opengl_render_primitive(CPU *cpu, Primitive *p);
void opengl_finish_frame(void);
void init_opengl_renderer(void);

GLuint opengl_get_shader_program(void);
