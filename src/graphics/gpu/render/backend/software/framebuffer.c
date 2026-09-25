#include "bus/bus.h"
#include "graphics/cp/cp.h"
#include "framebuffer.h"
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

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

static u32 read_z24(const u8 *efb, u32 x, u32 y)
{
    const u8 *p = efb + EFB_COLOR_BYTES + (y * EFB_WIDTH + x) * 3;
    return (u32)p[0] << 16 | (u32)p[1] << 8 | p[2];
}

u32 get_z_in_fb(CPU *cpu, s32 x, s32 y)
{
    const u32 pe_ctrl = get_bp_register_pointer()[0x43];
    const u32 fmt = pe_ctrl & 0x7;
    const u32 zfmt = (pe_ctrl >> 3) & 0x7;
    const u32 ux = (u32)x & 0x3FF;
    const u32 uy = (u32)y & 0x3FF;

    const u32 height = (fmt == PF_RGB565_Z16) ? EFB_HEIGHT_AA : EFB_HEIGHT;
    if (ux >= EFB_WIDTH || uy >= height) {
        return 0;
    }

    const u32 z = read_z24(cpu->bus->efb, ux, uy);

    switch (fmt) {
    case PF_RGB8_Z24:
    case PF_RGBA6_Z24:
    case PF_Z24:
        return z;

    case PF_RGB565_Z16: {
        const u32 z16 = z >> 8;
        return (z16 << 8) | (z16 >> 8);
    }

    case PF_Y8:
    case PF_U8:
    case PF_V8:
    case PF_YUV420:
    default:
        assert(!"fb format not impoemented \n");
        return z;
    }
}
