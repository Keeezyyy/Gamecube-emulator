#pragma once
#include "graphics/gpu/vertex/vertex_loader.h"

#define INITIAL_VERTEX_BUFFER_CAP 0x1000

typedef struct {
    u32 num;
    u32 cap;
    Vertex *buffer;
} VertexBuffer;

void init_vertex_buffer(VertexBuffer *self);
