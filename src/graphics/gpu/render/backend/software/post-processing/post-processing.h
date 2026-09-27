#pragma once

#include "graphics/gpu/render/backend/software/transform/transform.h"
void process_pixel(CPU *cpu, u32 x, u32 y, u32 z, RGBA color, RGBA texture_color, u32 ox, u32 oy);
