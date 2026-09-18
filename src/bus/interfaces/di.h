#pragma once

#include "cpu/cpu_types.h"
u64 di_read(CPU *cpu, u32 adr, u32 size);
void di_write(CPU *cpu, u32 adr, u32 val, u32 size);
