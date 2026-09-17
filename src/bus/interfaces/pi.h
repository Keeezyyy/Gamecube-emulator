#pragma once

#include "cpu/cpu_types.h"
u64 pi_read(CPU *cpu, u32 adr, u32 size);

void pi_write(CPU *cpu, u32 adr, u64 val, u32 size);
