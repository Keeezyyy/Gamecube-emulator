#include "texture.h"
#include "bus/bus.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void get_texture_unit_regs(TextureUnit *u, u8 tex_unit_num, const u32 *bp)
{
    const u8 idx = 0x80 | (tex_unit_num & 0x3) | ((tex_unit_num & 0x4) << 3);

    u32 *p = &u->mode0;
    for (int i = 0; i < 7; i++) {
        p[i] = bp[idx | (i << 2)];
    }
}

void load_texture_for_primitive(CPU *cpu, Vertex *v)
{
    const u32 *bp = get_bp_register_pointer();

    const u8 num_tex_gen = bp[0] & 0x7;

    const u8 num_tev_stages = ((bp[0] >> 10) & 0x7) + 1;
    if (!num_tex_gen) {
        // TODO: set no texture in vertex struct
        return;
    }

    for (int i = 0; i < num_tev_stages; i++) {
        const u32 tev_order = bp[0x28 + i];
        const bool is_even = i % 2 == 0;

        if (((tev_order >> TEV_ORDER_ENABLE_TEX(is_even)) & 1) == 0) {
            // tex nor enabled
            continue;
        }

        u8 tex_unit = (tev_order >> TEV_ORDER_TEX_MAP(is_even)) & 0x7;

        // decode_texture(cpu, 0x0, 0x0, bp, tex_unit);
    }
}

GXTexture decode_texture(CPU *cpu, const u32 *bp, TextureUnit u, u32 *width_o, u32 *height_o)
{

    const u32 ram_adr = (u.img3 & 0xFFFFFF);
    const u32 width = (u.img0 & 0x3FF) + 1;
    *width_o = width;
    const u32 height = (((u.img0 >> 10) & 0x3FF) + 1);
    *height_o = height;

    const u8 tex_format = ((u.img0 >> 20) & 0xF);

    if (tex_format == TEXTURE_FORMAT_I8) {
        return i8_decode(&cpu->bus->ram[ram_adr << 5], width, height);
    } else if (tex_format == TEXTURE_FORMAT_I4) {
        return i4_decode(&cpu->bus->ram[ram_adr << 5], width, height);
    } else if (tex_format == TEXTURE_FORMAT_RGBA8) {
        return rgba8_decode(&cpu->bus->ram[ram_adr << 5], width, height);

    } else {

        printf("texture format   0x%02x\n", (u.img0 >> 20) & 0xF);
        assert(!"texutre format isnt implemented\n");
    }
}

static s32 apply_wrap(const u8 wrap, u32 tex_size, s32 coord)
{
    s32 size = (s32)tex_size;
    switch (wrap) {
    case 0:
    case 3:
        if (coord < 0)
            return 0;
        if (coord > size - 1)
            return size - 1;
        return coord;

    case 1:
        return coord & (size - 1);

    case 2:
        if (coord & size)
            coord = ~coord;
        return coord & (size - 1);

    default:
        assert(!"wrong texture wrap");
        return 0;
    }
}

RGBA sample_texture(CPU *cpu, TextureUnit u, s32 texcoords[2], f32 lod)
{
    s8 lod_bias = (s8)(u.mode0 >> 9);
    lod_bias /= 2;

    lod += lod_bias;
    // if (lod <= 0) {
    if (true) {

        s32 s = texcoords[0] - 64;
        s32 t = texcoords[1] - 64;

        s32 s_coord = (s32)s / 128;
        s32 t_coord = (s32)t / 128;

        u32 width, height;
        GXTexture texture = decode_texture(cpu, get_bp_register_pointer(), u, &width, &height);

        RGBA *colors = (RGBA *)texture.buffer;

        u8 wrap_s = u.mode0 & 0x3;
        u8 wrap_t = (u.mode0 >> 2) & 0x3;

        bool mag_filter_linear = u.mode0 >> 4;
        // if (mag_filter_linear) {
        if (false) {
            // BIliniear
        } else {
            // near
            s_coord = apply_wrap(wrap_s, width, s_coord);
            t_coord = apply_wrap(wrap_t, height, t_coord);

            RGBA texel = colors[(t_coord * width + s_coord)];
            free(texture.buffer);
            return texel;
        }

    } else {
    }
}
