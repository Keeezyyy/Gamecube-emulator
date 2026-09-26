#pragma once

#include "graphics/gpu/render/backend/software/transform/transform.h"

typedef struct {
    s32 x;
    s32 y;
    u32 z;

    u8 colors[2][4];
    s32 tex[8][2];

    f32 lod[8];

    XFOutput *edges;
    Vertex *vert;
} PixelAttributes;
void rasterize_polygon(CPU *cpu, const XFOutput in[3], Vertex *v);
void rasterize_update_zslope(const XFOutput in[3]);
bool test_if_z_test_fails(CPU *cpu, u32 z, s32 x, s32 y);
