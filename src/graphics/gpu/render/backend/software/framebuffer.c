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

static inline u8 quantize(u8 v, u32 bits)
{
    const u32 q = v >> (8 - bits);
    return (u8)((q << (8 - bits)) | (q >> (2 * bits - 8)));
}

static void write_rgb8(u8 *efb, u32 x, u32 y, u8 r, u8 g, u8 b)
{
    u8 *p = efb + (y * EFB_WIDTH + x) * 3;
    p[0] = r;
    p[1] = g;
    p[2] = b;
}

static void write_z24(u8 *efb, u32 x, u32 y, u32 z)
{
    u8 *p = efb + EFB_COLOR_BYTES + (y * EFB_WIDTH + x) * 3;
    p[0] = (u8)(z >> 16);
    p[1] = (u8)(z >> 8);
    p[2] = (u8)z;
}

void write_to_fb(CPU *cpu, s32 x, s32 y, u8 r, u8 g, u8 b, u32 z)
{
    const u32 pe_ctrl = get_bp_register_pointer()[0x43];
    const u32 fmt = pe_ctrl & 0x7;
    const u32 ux = (u32)x & 0x3FF;
    const u32 uy = (u32)y & 0x3FF;

    const u32 height = (fmt == PF_RGB565_Z16) ? EFB_HEIGHT_AA : EFB_HEIGHT;
    if (ux >= EFB_WIDTH || uy >= height) {
        return;
    }

    u8 *efb = cpu->bus->efb;
    z &= 0xFFFFFF;

    switch (fmt) {
    case PF_RGB8_Z24:
        write_rgb8(efb, ux, uy, r, g, b);
        write_z24(efb, ux, uy, z);
        break;

    case PF_RGBA6_Z24:
        write_rgb8(efb, ux, uy, quantize(r, 6), quantize(g, 6), quantize(b, 6));
        write_z24(efb, ux, uy, z);
        break;

    case PF_RGB565_Z16:
        write_rgb8(efb, ux, uy, quantize(r, 5), quantize(g, 6), quantize(b, 5));
        // Nur die oberen 16 Bit sind relevant; get_z_in_fb liest z >> 8
        write_z24(efb, ux, uy, z & 0xFFFF00);
        break;

    case PF_Z24:
        // Reines Z-Format: nur Tiefe speichern
        write_z24(efb, ux, uy, z);
        break;

    case PF_Y8:
    case PF_U8:
    case PF_V8:
    case PF_YUV420:
    default:
        assert(!"fb format not implemented\n");
        break;
    }
}
