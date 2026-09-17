#pragma once

#include "cpu/cpu_types.h"

void si_write(CPU *cpu, u32 adr, u64 val, u32 size);
u64 si_read(CPU *cpu, u32 adr, u64 val);
