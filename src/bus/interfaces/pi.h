#pragma once

#include "cpu/cpu_types.h"
u32 pi_read(u32 adr);
u32 pi_write(CPU *cpu, u32 adr, u32 val);
