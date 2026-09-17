#pragma once

#include "cpu/cpu_types.h"
u64 exi_read(CPU *cpu, u32 adr, u32 size);
void exi_write(CPU *cpu, u32 adr, u64 val, u32 size);
