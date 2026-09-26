#pragma once

#include "core/config/config.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <assert.h>

typedef struct {
    u32 unused[3];
    u32 color;
    f32 cosatt[3];
    f32 distatt[3];
    f32 pos[3];
    f32 dir[3];
} XF_Light;

static_assert(sizeof(XF_Light) == 16 * 4, "XF_Light size");

typedef struct {
    f32 matrices[64][4];
    u32 unused_0100[0x300];
    f32 normal_matrices[32][3];
    u32 unused_0460[0xA0];
    f32 post_matrices[64][4];
    XF_Light lights[8];
} XF_Memory;

typedef struct {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} RGBA;

static_assert(sizeof(RGBA) == sizeof(u32), "RGBA BUFFER SIZE");

static_assert(sizeof(XF_Memory) == 0x680 * 4, "XF_Memory size");

typedef struct {
    u32 error;
    u32 diag_state_clock[4];
    u32 clip_disable;
    u32 perf0;
    u32 perf1;
    u32 vtx_specs;
    u32 num_channels;
    u32 ambient_color[2];
    u32 material_color[2];
    u32 channel_color[2];
    u32 channel_alpha[2];
    u32 dual_tex;
    u32 unknown_1013_1017[5];
    u32 matrix_index_a;
    u32 matrix_index_b;
    f32 viewport[6];
    f32 projection[6];
    u32 projection_type;
    u32 unknown_1027_103E[24];
    u32 num_tex_gens;
    u32 tex_mtx_info[8];
    u32 unknown_1048_104F[8];
    u32 post_mtx_info[8];
} XF_Registers;

typedef struct {
    Float4 pos;
    Float3 eye;
    Float3 NBT[3];
    Float3 tex[8];

    RGBA colors[2];
    XF_Light lights[2];

} XFOutput;

static_assert(sizeof(XF_Registers) == (0x1057 - 0x1000 + 1) * 4, "XF_Registers size");

void software_backend_write_to_xf_reg(const u32 reg_num, const u32 val);
XFOutput transform_vertex(CPU *cpu, const Vertex *v);

const XF_Registers *get_xf_regs(void);
const u32 *software_backend_get_xf_buffer(void);
