
#include "texture.h"
#include "graphics/gpu/render/backend/software/texture/texture.h"
#include "bus/bus.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define TEXTURE_STRIDE 1024

static inline u32 pack_rgba(u8 r, u8 g, u8 b, u8 a)
{
    return (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24);
}

static void _ia4_decode(const u8 *src, u32 width, u32 height, u32 *dest)
{

    u32 *pixels = dest;

    int counter = 0;
    for (int by = 0; by < height; by += 4) {
        for (int bx = 0; bx < width; bx += 8) {
            for (int ty = 0; ty < 4; ty++) {
                for (int tx = 0; tx < 8; tx++) {
                    u8 I = src[counter++];
                    int x = bx + tx, y = by + ty;
                    if (x >= width || y >= height)
                        continue;

                    const u8 c = (I & 0xF) * 0x11;
                    pixels[y * TEXTURE_STRIDE + x] = pack_rgba(c, c, c, I >> 4);
                }
            }
        }
    }
}
static void _i8_decode(const u8 *src, u32 width, u32 height, u32 *dest)
{
    u32 *pixels = dest;

    int counter = 0;
    for (int by = 0; by < height; by += 4) {
        for (int bx = 0; bx < width; bx += 8) {
            for (int ty = 0; ty < 4; ty++) {
                for (int tx = 0; tx < 8; tx++) {
                    u8 I = src[counter++];
                    int x = bx + tx, y = by + ty;
                    if (x >= width || y >= height)
                        continue;

                    pixels[y * TEXTURE_STRIDE + x] = pack_rgba(I, I, I, I);
                }
            }
        }
    }
}
static void _i4_decode(const u8 *src, u32 width, u32 height, u32 *dest)
{
    u32 *pixels = dest;
    int widthBlks = (width + 7) / 8;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int base = ((y / 8) * widthBlks + (x / 8)) * 32;
            int off = (y % 8) * 8 + (x % 8);
            u8 I = ((src[base + (off / 2)] >> (off % 2 == 1 ? 0 : 4)) & 0xF) * 0x11;

            pixels[y * TEXTURE_STRIDE + x] = pack_rgba(I, I, I, I);
        }
    }
}
static void _rgba8_decode(const u8 *src, u32 width, u32 height, u32 *dest)
{
    u32 *pixels = dest;
    int widthBlks = (width + 3) / 4;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int base = ((y / 4) * widthBlks + (x / 4)) * 64;
            int off = ((y % 4) * 4 + (x % 4)) * 2;

            pixels[y * TEXTURE_STRIDE + x] = pack_rgba(src[base + off + 1], src[base + 32 + off],
                                                       src[base + 32 + off + 1], src[base + off]);
        }
    }
}

static void _ia8_decode(const u8 *src, u32 width, u32 height, u32 *dest)
{

    u32 *pixels = dest;
    int widthBlks = (width + 3) / 4;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int base = ((y / 4) * widthBlks + (x / 4)) * 32;
            int off = ((y % 4) * 4 + (x % 4)) * 2;

            const u8 I = src[base + off + 1];
            pixels[y * TEXTURE_STRIDE + x] = pack_rgba(I, I, I, src[base + off]);
        }
    }
}

void opengl_encode_texture(CPU *cpu, TextureUnit *u, u32 *dest, u32 *upload_width,
                           u32 *upload_height)
{

    const u32 ram_adr = (u->img3 & 0xFFFFFF);
    const u32 width = (u->img0 & 0x3FF) + 1;
    const u32 height = (((u->img0 >> 10) & 0x3FF) + 1);
    const u8 tex_format = ((u->img0 >> 20) & 0xF);

    *upload_width = width;
    *upload_height = height;

    switch (tex_format) {
    case TEXTURE_FORMAT_I8:
        _i8_decode(&cpu->bus->ram[ram_adr << 5], width, height, dest);
        break;
    case TEXTURE_FORMAT_I4:
        _i4_decode(&cpu->bus->ram[ram_adr << 5], width, height, dest);
        break;
    case TEXTURE_FORMAT_RGBA8:
        _rgba8_decode(&cpu->bus->ram[ram_adr << 5], width, height, dest);
        break;
    case TEXTURE_FORMAT_IA4:
        _ia4_decode(&cpu->bus->ram[ram_adr << 5], width, height, dest);
        break;
    case TEXTURE_FORMAT_IA8:
        _ia8_decode(&cpu->bus->ram[ram_adr << 5], width, height, dest);
        break;
    default: {
        if (tex_format == TEXTURE_FORMAT_C4 || tex_format == TEXTURE_FORMAT_C8 ||
            tex_format == TEXTURE_FORMAT_C14X2) {

            u32 bw = 4, bh = 4;
            if (tex_format == TEXTURE_FORMAT_C4) {
                bw = 8;
                bh = 8;
            } else if (tex_format == TEXTURE_FORMAT_C8) {
                bh = 8;
            }

            const u32 bytes = ((width + bw - 1) / bw) * ((height + bh - 1) / bh) * 32;
            const u32 row_bytes = TEXTURE_STRIDE * sizeof(u32);

            memcpy(dest, &cpu->bus->ram[ram_adr << 5], bytes);
            *upload_width = TEXTURE_STRIDE;
            *upload_height = (bytes + row_bytes - 1) / row_bytes;
            return;
        }
        printf("texture format : 0x%x\n", tex_format);
        assert(!"texutre format isnt implemented\n");
    }
    }
}
