
#include "texture.h"
#include "graphics/gpu/render/backend/software/texture/texture.h"
#include "bus/bus.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

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

                    int i = (y * width + x) * 4;
                    pixels[i + 0] = pixels[i + 1] = pixels[i + 2] = (I & 0xF) * 0x11;
                    pixels[i + 3] = I >> 4;
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

                    int i = (y * width + x) * 4;
                    pixels[i + 0] = pixels[i + 1] = pixels[i + 2] = pixels[i + 3] = I;
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

            int i = (y * width + x) * 4;
            pixels[i + 0] = I; // R
            pixels[i + 1] = I; // G
            pixels[i + 2] = I; // B
            pixels[i + 3] = I; // A
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

            int i = (y * width + x) * 4;
            pixels[i + 0] = src[base + off + 1];
            pixels[i + 1] = src[base + 32 + off];
            pixels[i + 2] = src[base + 32 + off + 1];
            pixels[i + 3] = src[base + off];
        }
    }
}

static void _ia8_decode(const u8 *src, u32 width, u32 height, u32 *dest)
{

    u32 *pixels = dest;
    int widthBlks = (width + 3) / 4;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int base = ((y / 4) * widthBlks + (x / 4)) * 64;
            int off = ((y % 4) * 4 + (x % 4)) * 2;

            int i = (y * width + x) * 4;
            pixels[i + 0] = src[base + off + 1];
            pixels[i + 1] = src[base + 32 + off];
            pixels[i + 2] = src[base + 32 + off + 1];
            pixels[i + 3] = src[base + off];
        }
    }
}

void opengl_encode_texture(CPU *cpu, TextureUnit *u, u32 *dest)
{

    const u32 ram_adr = (u->img3 & 0xFFFFFF);
    const u32 width = (u->img0 & 0x3FF) + 1;
    const u32 height = (((u->img0 >> 10) & 0x3FF) + 1);
    const u8 tex_format = ((u->img0 >> 20) & 0xF);

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
        break;
    default: {
        if (tex_format == TEXTURE_FORMAT_C4 || tex_format == TEXTURE_FORMAT_C8 ||
            tex_format == TEXTURE_FORMAT_C14X2) {

            memcpy(dest, &cpu->bus->ram[ram_adr << 5], 1024 * 1024 * pow(2, (tex_format - 6)));
            return;
        }
        printf("texture format : 0x%x\n", tex_format);
        assert(!"texutre format isnt implemented\n");
    }
    }
}
