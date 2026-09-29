#include "texture.h"
#include "bus/bus.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static u8 tmem[0xFFFFF];

void load_tlut_into_tmem(CPU *cpu, const u32 ram_adr, const u32 tmem_adr,
                         const u32 num_of_32_byte_blocks)
{
    u32 corrected_adr = ((tmem_adr) << 9) + 0x80000;
    memcpy(&tmem[corrected_adr], &cpu->bus->ram[ram_adr << 5], num_of_32_byte_blocks / 32);
}

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

    switch (tex_format) {
    case TEXTURE_FORMAT_I8:
        return i8_decode(&cpu->bus->ram[ram_adr << 5], width, height);
    case TEXTURE_FORMAT_I4:
        return i4_decode(&cpu->bus->ram[ram_adr << 5], width, height);
    case TEXTURE_FORMAT_RGBA8:
        return rgba8_decode(&cpu->bus->ram[ram_adr << 5], width, height);
    case TEXTURE_FORMAT_IA4:
        return ia4_decode(&cpu->bus->ram[ram_adr << 5], width, height);
    default: {
        if (tex_format == TEXTURE_FORMAT_C4 || tex_format == TEXTURE_FORMAT_C8 ||
            tex_format == TEXTURE_FORMAT_C14X2) {
            return (GXTexture){width, height, NULL_PTR, tex_format};
        }
        printf("texture format   0x%02x\n", (u.img0 >> 20) & 0xF);
        assert(!"texutre format isnt implemented\n");
    }
    }

    return (GXTexture){0};
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

static u16 get_index_texture_index(s32 s, s32 t, u32 base_ram_adr, GXTexture *texture,
                                   const u8 *const ram)
{

    u32 bW, bH;
    switch (texture->format) {
    case TEXTURE_FORMAT_C4: {
        bW = 8;
        bH = 8;
        break;
    }
    case TEXTURE_FORMAT_C8: {
        bW = 4;
        bH = 8;
        break;
    }
    case TEXTURE_FORMAT_C14X2: {
        bW = 4;
        bH = 4;
        break;
    }
    default:
        assert(!"texture fnaklsd ");
    }
    u32 width_block = (texture->width + bW - 1) / bW;
    u32 blk = (t / bH) * width_block + (s / bW);
    u32 off = (t % bH) * bW + (s % bW);

    if (texture->format == TEXTURE_FORMAT_C4) {
        return ram[base_ram_adr + blk * 32 + off / 2] & 1
                   ? ram[base_ram_adr + blk * 32 + off / 2] & 0xF
                   : ram[base_ram_adr + blk * 32 + off / 2] >> 4;
    } else if (texture->format == TEXTURE_FORMAT_C8) {
        return ram[base_ram_adr + blk * 32 + off / 2];
    } else {
        return *((u16 *)&ram[base_ram_adr + blk * 32 + off / 2]);
    }
}

static inline u8 expand(u32 v, u32 bits)
{
    v &= (1u << bits) - 1;
    switch (bits) {
    case 3:
        return (u8)((v << 5) | (v << 2) | (v >> 1));
    case 4:
        return (u8)(v * 0x11);
    case 5:
        return (u8)((v << 3) | (v >> 2));
    case 6:
        return (u8)((v << 2) | (v >> 4));
    default:
        return (u8)v;
    }
}
static RGBA decode_tlut_color(const u16 color, const u8 format)
{
    switch (format) {
    case 0: {
        return (RGBA){.r = color & 0xff, .g = color & 0xff, .b = color & 0xff, .a = color >> 8};
        break;
    } // I8
    case 1: {
        return (RGBA){.r = expand(color >> 11, 5),
                      .g = expand(color >> 5, 6),
                      .b = expand(color, 5),
                      .a = 0xFF};
        break;
    } // RGBA565
    case 2: {
        if (color >> 15) {
            return (RGBA){.r = expand(color >> 10, 5),
                          .g = expand(color >> 5, 5),
                          .b = expand(color, 5),
                          .a = 0xFF};
        } else {
            return (RGBA){.r = expand(color >> 8, 4),
                          .g = expand(color >> 4, 4),
                          .b = expand(color, 4),
                          .a = expand(color >> 12, 3)};
        }

        break;
    } // RGBA565
    default:
        assert(!"decode tlut clolor\n");
    }
}

static RGBA get_texel_from_texture(CPU *cpu, const TextureUnit *const u, s32 s, s32 t,
                                   GXTexture *texture)
{
    if (texture->format <= TEXTURE_FORMAT_RGBA8) {
        if (texture->buffer == NULL_PTR)
            return (RGBA){0};

        RGBA texel = ((RGBA *)texture->buffer)[(t * texture->width + s)];
        return texel;
    }

    const u32 ram_adr = (u->img3 & 0xFFFFFF) << 5;
    const u16 idx = get_index_texture_index(s, t, ram_adr, texture, cpu->bus->ram);

    const u32 tmem_adr = (u->lut & 0x3FF) + 0x80000;

    const u16 color = ((u16 *)&tmem[tmem_adr])[idx];

    return decode_tlut_color(color, (u->lut >> 10) & 0x3);
}

RGBA sample_texture(CPU *cpu, TextureUnit u, s32 texcoords[2], f32 lod)
{
    lod += (f32)(s8)((u.mode0 >> 9) & 0xFF) / 32.0f;
    lod = fminf(lod, (f32)((u.mode1 >> 8) & 0xFF) / 16.0f);
    lod = fmaxf(lod, (f32)(u.mode1 & 0xFF) / 16.0f);
    // if (lod <= 0) {
    if (true) {

        s32 s_coord = texcoords[0] >> 7;
        s32 t_coord = texcoords[1] >> 7;

        u32 width, height;
        GXTexture texture = decode_texture(cpu, get_bp_register_pointer(), u, &width, &height);

        RGBA *colors = (RGBA *)texture.buffer;
        if (!colors)
            return (RGBA){0};

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

            return get_texel_from_texture(cpu, &u, s_coord, t_coord, &texture);
        }

    } else {
    }

    return (RGBA){0};
}
