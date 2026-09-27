#pragma once
#include "bus/bus.h"
#include "graphics/gpu/vertex/primitive.h"
#include "graphics/gpu/vertex/vertex_loader.h"

#define SOFTWARE_TRANSFORM

void load_vertex_into_pipeline(CPU *cpu, Primitive p);
