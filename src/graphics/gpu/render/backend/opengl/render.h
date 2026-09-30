#pragma once

#include "bus/bus.h"
#include "graphics/gpu/vertex/primitive.h"

enum {
    GPUV_FLAG_POS_3D = 1u << 0,
    GPUV_FLAG_NORMAL = 1u << 1,
    GPUV_FLAG_NBT = 1u << 2,
    GPUV_FLAG_COLOR0 = 1u << 3,
    GPUV_FLAG_COLOR1 = 1u << 4,
    GPUV_TEX_SHIFT = 8,     // Bits  8..15: has_texture[i]
    GPUV_TEXMAT_SHIFT = 16, // Bits 16..23: has_tex_mat_idx[i]
    GPUV_POSMAT_SHIFT = 24, // Bits 24..31: posMatId
};

typedef struct {
    float pos[3];
    float normal[3];
    float binormal[3];
    float tangent[3];
    u8 color[2][4];
    float tex[8][2];
    Float4 pos_mat[3];
    u32 flags;
    u32 tex_mat_idx[2];
} GpuVertex;

_Static_assert(offsetof(GpuVertex, color) % 4 == 0, "align");
_Static_assert(sizeof(GpuVertex) % 4 == 0, "align");

void opengl_render_primitive(CPU *cpu, Primitive *p);
void opengl_finish_frame(void);
void init_opengl_renderer(void);
