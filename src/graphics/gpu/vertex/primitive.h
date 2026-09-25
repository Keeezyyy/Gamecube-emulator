#pragma once

#include "graphics/gpu/vertex/vertex_loader.h"
#include "utils/vector.h"
typedef struct {
    Vertex *vertecies;
    u16 vert_count;

    PrimitiveType t;

} Primitive;
