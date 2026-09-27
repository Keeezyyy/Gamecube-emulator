#include "post-processing.h"
#include "bus/bus.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/framebuffer.h"
#include "graphics/gpu/render/backend/software/rasterize/rasterize.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"

#ifdef RENDER_TEST_RAYLIB
#include <raylib.h>
#endif

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

    if (operation == 0)
        return z;

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

typedef enum { ZERO, ONE, SRCCLR, INVSRCCLR, SRCALPHA, INVSRCALPHA, DSTALPHA, INVDSTALPHA } Factor;
static u16 _get_factor_num(Factor f, const RGBA src, const RGBA dest, const u8 chnl)
{
    switch (f) {
    case ZERO:
        return 0;
    case ONE:
        return 255;
    case SRCCLR:
        return ((u8 *)&src)[chnl];
    case INVSRCCLR:
        return 255 - ((u8 *)&src)[chnl];
    case SRCALPHA:

        return src.a;
    case DSTALPHA:
        return dest.a;
    case INVSRCALPHA:
        return 255 - src.a;
    case INVDSTALPHA:
        return 255 - dest.a;
    }
}

static u8 clamp(s16 val)
{
    return val < 0 ? 0 : val > 255 ? 255 : (u8)val;
}

static RGBA _blend(RGBA fb_color, RGBA new_color)
{
    const u32 ctrl = get_bp_register_pointer()[0x41];

    RGBA out = new_color;

    for (u8 i = 0; i < 4; i++) {
        if (ctrl & 1) {
            if ((ctrl >> 11) & 1) { // subtract 1
                ((u8 *)&out)[i] = clamp((s16)((((u8 *)&fb_color)[i] - ((u8 *)&new_color)[i])));
            } else {

                const u8 src_sel = (ctrl >> 8) & 0x7;
                u16 src_factor = _get_factor_num(
                    src_sel, src_sel == 2 || src_sel == 3 ? fb_color : new_color, fb_color, i);
                src_factor = src_factor + src_factor / 128; // expand 0-255 to 0-256

                u16 dst_factor = _get_factor_num((ctrl >> 5) & 0x7, new_color, fb_color, i);
                dst_factor = dst_factor + dst_factor / 128; // expand 0-255 to 0-256

                const s16 blended = (s16)((((u8 *)&new_color)[i] * src_factor +
                                           ((u8 *)&fb_color)[i] * dst_factor) >>
                                          8);
                ((u8 *)&out)[i] = clamp(blended);
            }
        } else if ((ctrl >> 1) & 1) {
            const u8 operation = (ctrl >> 12) & 0xF;
            switch (operation) {
            case 0: // CLEAR: 0
                ((u8 *)&out)[i] = 0;
                break;

            case 1: // AND: s und d
                ((u8 *)&out)[i] = ((u8 *)&new_color)[i] & ((u8 *)&fb_color)[i];
                break;

            case 2: // REVAND: s und nicht d
                ((u8 *)&out)[i] = ((u8 *)&new_color)[i] & ~((u8 *)&fb_color)[i];
                break;

            case 3: // COPY: s
                ((u8 *)&out)[i] = ((u8 *)&new_color)[i];
                break;

            case 4: // INVAND: nicht s und d
                ((u8 *)&out)[i] = ~((u8 *)&new_color)[i] & ((u8 *)&fb_color)[i];
                break;

            case 5: // NOOP: d
                ((u8 *)&out)[i] = ((u8 *)&fb_color)[i];
                break;

            case 6: // XOR: s xor d
                ((u8 *)&out)[i] = ((u8 *)&new_color)[i] ^ ((u8 *)&fb_color)[i];
                break;

            case 7: // OR: s oder d
                ((u8 *)&out)[i] = ((u8 *)&new_color)[i] | ((u8 *)&fb_color)[i];
                break;

            case 8: // NOR: nicht (s oder d)
                ((u8 *)&out)[i] = ~(((u8 *)&new_color)[i] | ((u8 *)&fb_color)[i]);
                break;

            case 9: // EQUIV: nicht (s xor d)
                ((u8 *)&out)[i] = ~(((u8 *)&new_color)[i] ^ ((u8 *)&fb_color)[i]);
                break;

            case 10: // INV: nicht d
                ((u8 *)&out)[i] = ~((u8 *)&fb_color)[i];
                break;

            case 11: // REVOR: s oder nicht d
                ((u8 *)&out)[i] = ((u8 *)&new_color)[i] | ~((u8 *)&fb_color)[i];
                break;

            case 12: // INVCOPY: nicht s
                ((u8 *)&out)[i] = ~((u8 *)&new_color)[i];
                break;

            case 13: // INVOR: nicht s oder d
                ((u8 *)&out)[i] = ~((u8 *)&new_color)[i] | ((u8 *)&fb_color)[i];
                break;

            case 14: // NAND: nicht (s und d)
                ((u8 *)&out)[i] = ~(((u8 *)&new_color)[i] & ((u8 *)&fb_color)[i]);
                break;

            case 15: // SET: alle Bits 1
                ((u8 *)&out)[i] = 0xFF;
                break;
            }
        }
    }

    return out;
}

static void _alpha_override(RGBA *color)
{
    if ((get_bp_register_pointer()[0x42] >> 8) & 1) {
        color->a = get_bp_register_pointer()[0x42] & 0xFF;
    }
}

void process_pixel(CPU *cpu, u32 x, u32 y, u32 z, RGBA color, RGBA texture_color, u32 ox, u32 oy)
{
    u32 new_z = _z_texture(texture_color, z);

    _fog();

    if (!((get_bp_register_pointer()[0x43] >> 6) & 1) &&
        test_if_z_test_fails(cpu, new_z, (s32)x, (s32)y))
        return;

    _update_bbox(x, y);

    RGBA fb_color = read_from_fb((s32)x, (s32)y);

    RGBA new_color = _blend(fb_color, color);

    _alpha_override(&new_color);

#ifdef RENDER_TEST_RAYLIB
    DrawPixel(x + ox - 342, y + oy - 342, (Color){new_color.r, new_color.g, new_color.b, 255});
#endif
    write_to_fb(x, y, new_color.r, new_color.g, new_color.b,
                (get_bp_register_pointer()[0x40] & 0x11) == 0x11
                    ? new_z
                    : get_z_in_fb(cpu, (s32)x, (s32)y));
}
