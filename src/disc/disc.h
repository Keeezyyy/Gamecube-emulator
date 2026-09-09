#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char game_code[4];
    char maker_code[2];
    uint8_t disc_id;
    uint8_t version;
    uint8_t audio_streaming;
    uint8_t stream_buffer_size;
    uint8_t unused_000a[0x12];
    uint32_t dvd_magic_word;
    char game_name[0x3E0];
    uint32_t debug_monitor_offset;
    uint32_t debug_monitor_address;
    uint8_t unused_0408[0x18];
    uint32_t dol_offset;
    uint32_t fst_offset;
    uint32_t fst_size;
    uint32_t fst_max_size;
    uint32_t user_position;
    uint32_t user_length;
    uint32_t unknown_0438;
    uint8_t unused_043c[4];

} DiscHeader;

typedef struct Disc Disc;

struct Disc {
    DiscHeader *header;
    void *rom;
    size_t rom_size;

    int (*load_rom)(Disc *self, char *path);
    void (*free)(Disc *self);
    void (*print_header)(Disc *self);
};

/*
Boot.bin (Data Header)
Item No.	Offset	Length	Name
    1	0x0	0x4	ID
    2	0x4	0x2	Maker Code
    3	0x5	0x1	Disc No
    4	0x6	0x1	Disc Version
    5	0x20	0x40	Title
    6	0x420	0x4	Main.dol location
    7	0x424	0x4	Fst.bin location
    8	0x428	0x4	Fst.bin size
    9	0x42C	0x4	Max Fst.bin size

*/

void init_disc(Disc *self);
