#pragma once
#include "graphics/gpu/render/backend/software/rasterize/rasterize.h"
typedef enum { Color0 = 0, Color1 = 1, AlphaBump = 5, AlphaBumpN = 6, Zero = 7 } ColorType;

void tev_write_color_reg(u8 reg, u32 value);

void draw_pixel(CPU *cpu, const PixelAttributes *p, u32 x, u32 y, u32 ox, u32 oy);
