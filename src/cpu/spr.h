#pragma once
#include "cpu.h"

#define SPR_DECREMENTOR 22

#define SPR_TIME_BASE_H 269
#define SPR_TIME_BASE_L 268

u32 time_base_read(CPU *cpu, const u32 adr);

void time_base_write(CPU *cpu, const u32 adr, const u32 val);
