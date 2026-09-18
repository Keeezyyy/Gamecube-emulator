#include "disc.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
static uint32_t be32(uint32_t x)
{
    return ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) | ((x & 0x00FF0000) >> 8) |
           ((x & 0xFF000000) >> 24);
}
static void _print_rom_header(Disc *self)
{
    DiscHeader *h = self->header;
    DEBUG_PRINT("=== DiscHeader ===\n");

    DEBUG_PRINT("game_code           : %.4s\n", h->game_code);
    DEBUG_PRINT("maker_code          : %.2s\n", h->maker_code);
    DEBUG_PRINT("disc_id             : 0x%02X (%u)\n", h->disc_id, h->disc_id);
    DEBUG_PRINT("version             : 0x%02X (%u)\n", h->version, h->version);
    DEBUG_PRINT("audio_streaming     : 0x%02X (%u)\n", h->audio_streaming, h->audio_streaming);
    DEBUG_PRINT("stream_buffer_size  : 0x%02X (%u)\n", h->stream_buffer_size,
                h->stream_buffer_size);
    DEBUG_PRINT("dvd_magic_word      : 0x%08X\n", be32(h->dvd_magic_word));

    DEBUG_PRINT("dol_offset          : 0x%08X\n", be32(h->dol_offset));

    DEBUG_PRINT("fst_offset          : 0x%08X\n", be32(h->fst_offset));

    DEBUG_PRINT("fst_size            : 0x%08X (%u)\n", be32(h->fst_size), be32(h->fst_size));

    DEBUG_PRINT("fst_max_size        : 0x%08X (%u)\n", be32(h->fst_max_size),
                be32(h->fst_max_size));
    DEBUG_PRINT("user_position       : 0x%08X\n", h->user_position);
    DEBUG_PRINT("user_length         : 0x%08X (%u)\n", h->user_length, h->user_length);

    DEBUG_PRINT("unknown_0438        : 0x%08X\n", h->unknown_0438);

    DEBUG_PRINT("==================\n");
}

static int _load_rom(Disc *self, char *path)
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

    self->rom = (uint8_t *)buffer;
    self->header = (DiscHeader *)buffer;
    self->rom_size = bytes_read;

    fclose(f);

    return 0;
}
static void _free(Disc *self)
{
    free(self->rom);
}

static const Disc DISC_TEMPLATE = {
    .load_rom = &_load_rom, .free = &_free, .print_header = &_print_rom_header};

void init_disc(Disc *self)
{
    *self = DISC_TEMPLATE;
}
