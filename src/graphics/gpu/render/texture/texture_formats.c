
#include <_abort.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "texture.h"
#include <stb/stb_image_write.h>

static u8 *p_buffer[1000];
static u32 g_counter = 0;

void i8_decode(const u8 *src, u32 width, u32 height)
{
    for (int i = 0; i < g_counter; i++) {
        if (p_buffer[i] == src) {
            return;
        }
    }

    p_buffer[g_counter++] = src;
    if (g_counter == 1000)
        assert(!"limit reached\n");

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

    char buffer[128];
    sprintf(buffer, "texture-output/i8-output-%p.png", src);
    stbi_write_png(buffer, width, height, 4, pixels, width * 4);

    free(pixels);
}
void i4_decode(const u8 *src, u32 width, u32 height)
{
    for (int i = 0; i < g_counter; i++) {
        if (p_buffer[i] == src) {
            return;
        }
    }

    p_buffer[g_counter++] = src;
    if (g_counter == 1000)
        assert(!"limit reached\n");

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
    char buffer[1024];
    sprintf(buffer, "texture-output/i4-output-%p.png", src);
    stbi_write_png(buffer, width, height, 4, pixels, width * 4);

    free(pixels);
}
void rgba8_decode(const u8 *src, u32 width, u32 height)
{
    for (int i = 0; i < g_counter; i++) {
        if (p_buffer[i] == src) {
            return;
        }
    }

    p_buffer[g_counter++] = src;
    if (g_counter == 1000)
        assert(!"limit reached\n");

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
    char buffer[1024];
    sprintf(buffer, "texture-output/rgba8-output-%p.png", src);
    stbi_write_png(buffer, width, height, 4, pixels, width * 4);

    free(pixels);
}
