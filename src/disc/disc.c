#include "disc.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static uint8_t *rom_binary;
static size_t rom_size_in_bytes;

static DiscHeader *disc_header;

static void _print_rom_header()
{
    for (int i = 0; i < sizeof(disc_header->game_name); i++) {
        putchar(disc_header->game_name[i]);
    }
}

int load_rom(char *path)
{
    FILE *f = fopen(path, "rb");

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size);
    if (!buffer) {
        fclose(f);
        return 1;
    }

    size_t bytes_read = fread(buffer, 1, size, f);

    rom_binary = (uint8_t *)buffer;
    disc_header = (DiscHeader *)buffer;
    rom_size_in_bytes = bytes_read;

    fclose(f);

    _print_rom_header();
    return 0;
}
