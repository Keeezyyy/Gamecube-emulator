#pragma once

#include "bus/bus.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"

#define EFB_WIDTH 640
#define EFB_HEIGHT 528
#define EFB_HEIGHT_AA 264
#define EFB_COLOR_BYTES (EFB_WIDTH * EFB_HEIGHT * 3)

enum {
    PF_RGB8_Z24 = 0,
    PF_RGBA6_Z24 = 1,
    PF_RGB565_Z16 = 2,
    PF_Z24 = 3,
    PF_Y8 = 4,
    PF_U8 = 5,
    PF_V8 = 6,
    PF_YUV420 = 7,
};

enum { ZC_LINEAR = 0, ZC_NEAR = 1, ZC_MID = 2, ZC_FAR = 3 };

u32 get_z_in_fb(CPU *cpu, s32 x, s32 y);
void write_to_fb(s32 x, s32 y, u8 r, u8 g, u8 b, u32 z);

RGBA read_from_fb(s32 x, s32 y);
