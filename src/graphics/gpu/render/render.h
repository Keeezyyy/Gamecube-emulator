#pragma once
#include "core/config/config.h"
#include "graphics/gpu/vertex/vertex_loader.h"

#define INITIAL_VERTEX_BUFFER_CAP 0x1000
#define MAX_RENDER_VERTICES 10000

typedef union {
    u32 bp_registers[256];
} PACKED BPRegisters;

typedef union {
    u32 cp_registers[256];
} PACKED CPRegisters;

typedef union {
    u32 xf_registers[0x1057];
} PACKED XFRegisters;

typedef struct {
    f32 x;
    f32 y;
    f32 z;
} Float3;

typedef struct {
    Float3 pos;

    u8 is_3d;
    u8 has_pos_mat_idx;
    u16 pos_mat_idx;
} GpuPos;

typedef struct {
    GpuPos pos;

} PACKED GpuVertex;

void init_renderer(void);
void push_vertex_to_vertex_buffer(Vertex v);

void cpy_reg_state(u32 *bp_s, u32 *cp_s, u32 *xf_s);
