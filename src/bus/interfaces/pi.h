#pragma once

#include "cpu/cpu_types.h"
u64 *pi_read(u32 adr);

u64 *pi_write(CPU *cpu, u32 adr);
