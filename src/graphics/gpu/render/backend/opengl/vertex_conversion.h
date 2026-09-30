#pragma once
#include "graphics/gpu/render/backend/opengl/render.h"

GpuVertex vertex_to_gpu(const Vertex *v);
void gpu_vertex_setup_attribs(void);
