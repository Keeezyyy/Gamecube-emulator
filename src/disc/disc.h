#pragma once

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
int load_rom(char *path);
