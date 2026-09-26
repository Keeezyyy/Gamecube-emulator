#pragma once

#include "bus/bus.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/vertex/vertex_loader.h"

#define TEV_ORDER_ENABLE_TEX(is_even) (is_even ? 6 : 18)
#define TEV_ORDER_TEX_MAP(is_even) (is_even ? 0 : 12)
#define TEV_TEX_CORD(is_even) (is_even ? 3 : 15)
#define TEV_COLOR_CHANNEL(is_even) (is_even ? 7 : 19)

typedef enum {
    TEXTURE_FORMAT_I4 = 0x0,
    TEXTURE_FORMAT_I8 = 0x1,
    TEXTURE_FORMAT_IA4 = 0x2,
    TEXTURE_FORMAT_IA8 = 0x3,
    TEXTURE_FORMAT_RGB565 = 0x4,
    TEXTURE_FORMAT_RGB5A3 = 0x5,
    TEXTURE_FORMAT_RGBA8 = 0x6,

    TEXTURE_FORMAT_C4 = 0x8,
    TEXTURE_FORMAT_C8 = 0x9,
    TEXTURE_FORMAT_C14X2 = 0xA,
    TEXTURE_FORMAT_CMPR = 0xE,
} TextureFormat;

typedef struct {
    u32 mode0, mode1, img0, img1, img2, img3, lut;
} TextureUnit;

typedef struct {
    u32 width;
    u32 height;
    u8 *buffer;
} GXTexture;

GXTexture decode_texture(CPU *cpu, const u32 *bp, TextureUnit u, u32 *width_o, u32 *height_o);

void get_hash_from_bp_stat(const u32 *bp, char *dest);

void load_texture(const u8 tex_usage_bitmap, const char *hash);

void load_texture_for_primitive(CPU *cpu, Vertex *v);

void get_texture_unit_regs(TextureUnit *u, u8 tex_unit_num, const u32 *bp);

GXTexture i8_decode(const u8 *src, u32 width, u32 height);
GXTexture i4_decode(const u8 *src, u32 width, u32 height);
GXTexture rgba8_decode(const u8 *src, u32 width, u32 height);

RGBA sample_texture(CPU *cpu, TextureUnit u, s32 texcoords[2], f32 lod);

void init_texture_hash_map(void);
