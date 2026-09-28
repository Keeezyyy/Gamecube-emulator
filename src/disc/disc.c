#include "disc.h"
#include <_abort.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static int _load_rom(Disc *self, char *path)
{
    FILE *f = fopen(path, "rb");

    fseek(f, 0, SEEK_END);
    long size = ftell(f);

    fseek(f, 0, SEEK_SET);

    self->rom_size = (u64)size;
    self->game_file = f;

    return 0;
}
static void _free(Disc *self)
{
    fclose(self->game_file);
}

static void _read(Disc *self, void *dest, u32 disc_offset, u32 len)
{
    if (disc_offset > self->rom_size) {
        perror("[DISC] : Error reading disc\n");
        // TODO: pass error on to the di interface
        abort();
    }

    fseek(self->game_file, disc_offset, SEEK_SET);

    fread(dest, 1, len, self->game_file);
}

static const Disc DISC_TEMPLATE = {.load_rom = &_load_rom, .free = &_free, .read = &_read};

void init_disc(Disc *self)
{
    *self = DISC_TEMPLATE;
}
