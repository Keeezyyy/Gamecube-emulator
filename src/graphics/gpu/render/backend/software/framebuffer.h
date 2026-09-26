#pragma once

#include "bus/bus.h"
u32 get_z_in_fb(CPU *cpu, s32 x, s32 y);

void write_to_fb(CPU *cpu, s32 x, s32 y, u8 r, u8 g, u8 b, u32 z);
