#include <unistd.h>
#include <_abort.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "core/config/config.h"
#include "texture.h"

#include <khash/khash.h>
#define OUTPUT_TEXTURE

#ifdef OUTPUT_TEXTURE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#endif

KHASH_MAP_INIT_INT64(tex_map, u8 *)

static khash_t(tex_map) * t;

void init_texture_hash_map(void)
{
    t = kh_init(tex_map);
}

static inline u64 get_texture_hash(const u8 *src, const u32 width, const u32 height)
{
    return (u64)(u32)(uintptr_t)src << 32 | (width & 0xFFFF) << 16 | (height & 0xFFFF);
}

static inline u8 *texture_lookup(const u64 hash)
{
    const khiter_t k = kh_get(tex_map, t, hash);
    return k == kh_end(t) ? NULL_PTR : kh_value(t, k);
}

static inline void texture_insert(const u64 hash, u8 *pixels)
{
    int ret;
    const khiter_t k = kh_put(tex_map, t, hash, &ret);
    if (ret == -1)
        assert(!"khash error ");

    kh_value(t, k) = pixels;
}

GXTexture ia4_decode(const u8 *src, u32 width, u32 height)
{
    const u64 hash = get_texture_hash(src, width, height);
    u8 *p = texture_lookup(hash);

    if (p != NULL_PTR) {
        GXTexture out = {0};
        out.format = TEXTURE_FORMAT_IA4;
        out.buffer = p;
        out.height = height;
        out.width = width;
        return out;
    }

    const int channels = 4;

    unsigned char *pixels = malloc(width * height * channels);

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

#ifdef OUTPUT_TEXTURE
    char buffer[128];
    sprintf(buffer, "texture-output/ia4-output-%p-%ux%u.png", src, width, height);

    if (!access(buffer, F_OK) == 0)
        stbi_write_png(buffer, width, height, 4, pixels, width * 4);
#endif

    GXTexture out = {0};
    out.buffer = pixels;
    out.height = height;
    out.width = width;

    texture_insert(hash, pixels);
    return out;
}
GXTexture i8_decode(const u8 *src, u32 width, u32 height)
{
    const u64 hash = get_texture_hash(src, width, height);
    u8 *p = texture_lookup(hash);

    if (p != NULL_PTR) {
        GXTexture out = {0};
        out.format = TEXTURE_FORMAT_I8;
        out.buffer = p;
        out.height = height;
        out.width = width;
        return out;
    }

    const int channels = 4;

    unsigned char *pixels = malloc(width * height * channels);

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

#ifdef OUTPUT_TEXTURE
    char buffer[128];
    sprintf(buffer, "texture-output/i8-output-%p-%ux%u.png", src, width, height);

    if (!access(buffer, F_OK) == 0)
        stbi_write_png(buffer, width, height, 4, pixels, width * 4);
#endif

    GXTexture out = {0};
    out.buffer = pixels;
    out.height = height;
    out.width = width;

    texture_insert(hash, pixels);
    return out;
}
GXTexture i4_decode(const u8 *src, u32 width, u32 height)
{
    const u64 hash = get_texture_hash(src, width, height);
    u8 *p = texture_lookup(hash);

    if (p != NULL_PTR) {
        GXTexture out = {0};

        out.format = TEXTURE_FORMAT_I4;
        out.buffer = p;
        out.height = height;
        out.width = width;
        return out;
    }

    const int channels = 4;

    unsigned char *pixels = malloc(width * height * channels);
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

#ifdef OUTPUT_TEXTURE
    char buffer[128];
    sprintf(buffer, "texture-output/i4-output-%p-%ux%u.png", src, width, height);

    if (access(buffer, F_OK) != 0)
        stbi_write_png(buffer, width, height, 4, pixels, width * 4);

#endif

    GXTexture out = {0};
    out.buffer = pixels;
    out.height = height;
    out.width = width;

    texture_insert(hash, pixels);
    return out;
}
GXTexture rgba8_decode(const u8 *src, u32 width, u32 height)
{
    const u64 hash = get_texture_hash(src, width, height);
    u8 *p = texture_lookup(hash);

    if (p != NULL_PTR) {
        GXTexture out = {0};
        out.format = TEXTURE_FORMAT_RGBA8;
        out.buffer = p;
        out.height = height;
        out.width = width;
        return out;
    }

    const int channels = 4;

    unsigned char *pixels = malloc(width * height * channels);
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

#ifdef OUTPUT_TEXTURE
    char buffer[128];
    sprintf(buffer, "texture-output/rgba8-output-%p-%ux%u.png", src, width, height);

    if (!access(buffer, F_OK) == 0)
        stbi_write_png(buffer, width, height, 4, pixels, width * 4);
#endif

    GXTexture out = {0};
    out.buffer = pixels;
    out.height = height;
    out.width = width;

    texture_insert(hash, pixels);
    return out;
}
