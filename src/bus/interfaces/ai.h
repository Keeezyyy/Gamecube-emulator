#pragma once

#include "cpu/cpu_types.h"
u64 ai_read(CPU *cpu, u32 adr, u32 size);
void ai_write(CPU *cpu, u32 adr, u64 val, u32 size);

void ai_write_to_streaming_interface(CPU *cpu, u32 adr, u64 val, u32 size);

u64 ai_read_from_streaming_interface(CPU *cpu, u32 adr, u32 size);
