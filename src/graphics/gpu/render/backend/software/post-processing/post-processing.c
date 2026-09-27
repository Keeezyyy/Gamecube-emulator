#include "post-processing.h"
#include "bus/bus.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/rasterize/rasterize.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"

typedef enum {
    U8 = 0,
    U16 = 1,
    U24 = 2,

} ZTextureFmt;

static u32 _z_texture(RGBA raw_tex, u32 z)
{
    const u32 bias = get_bp_register_pointer()[0xF4] & 0xFFFFFF;
    const ZTextureFmt fmt = get_bp_register_pointer()[0xF5] & 0x3;
    const u8 operation = (get_bp_register_pointer()[0xF5] >> 2) & 0x3;

    u32 t;
    switch (fmt) {
    case U8:
        t = raw_tex.a;
        break;
    case U16:
        t = (u32)(raw_tex.a << 8) | raw_tex.r;
        break;
    default:
        t = (u32)(raw_tex.r << 16) | (u32)(raw_tex.g << 8) | raw_tex.b;
        break;
    }

    t += bias;

    return ((operation == 1) ? z + t : t) & 0xFFFFFF;
}

static void _fog()
{
}

static void _update_bbox(u32 x, u32 y)
{
    u16 *bbox = get_bbox();

    bbox[0] = bbox[0] > x ? (u16)x : bbox[0];
    bbox[1] = bbox[1] < x ? (u16)x : bbox[1];
    bbox[2] = bbox[2] > y ? (u16)y : bbox[2];
    bbox[3] = bbox[3] < y ? (u16)y : bbox[3];
}

void process_pixel(CPU *cpu, u32 x, u32 y, u32 z, RGBA color, RGBA texture_color)
{
    u32 new_z = _z_texture(texture_color, z);

    _fog();

    if (!((get_bp_register_pointer()[0x43] >> 6) & 1) &&
        test_if_z_test_fails(cpu, new_z, (s32)x, (s32)y))
        return;

    _update_bbox(x, y);
}
