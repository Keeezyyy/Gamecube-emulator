#pragma once
#include "core/config/config.h"
#include "graphics/gpu/vertex/vertex_loader.h"

#define INITIAL_VERTEX_BUFFER_CAP 0x1000

typedef union {
    u32 bp_registers[256];
} PACKED BPRegisters;

typedef union {
    u32 cp_registers[256];
} PACKED CPRegisters;

typedef union {
    u32 xf_registers[0x1057];
} PACKED XFRegisters;

void init_renderer(void);
void push_vertex_to_vertex_buffer(Vertex v);

void cpy_reg_state(u32 *bp_s, u32 *cp_s, u32 *xf_s);
