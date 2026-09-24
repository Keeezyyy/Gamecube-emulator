
#include <_abort.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "texture.h"
#include <stb/stb_image_write.h>

static u8 *buffer[100];
static u32 g_counter = 0;

void i8_decode(const u8 *src, u32 width, u32 height)
{
    for (int i = 0; i < g_counter; i++) {
        if (buffer[i] == src) {
            return;
        }
    }

    buffer[g_counter++] = src;

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
    sprintf(buffer, "texture-output/output-%d.png", src);
    stbi_write_png(buffer, width, height, 4, pixels, width * 4);

    free(pixels);
}
