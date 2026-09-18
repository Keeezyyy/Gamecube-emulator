#pragma once

#include "cpu/cpu_types.h"

#define SI_CMD_STATUS_REQ 0x00

#define SI_TYPE_GC_CONTROLLER 0x09000000
#define SI_TYPE_NOT_CONNECTED 0x00000008

void si_write(CPU *cpu, u32 adr, u64 val, u32 size);
u64 si_read(CPU *cpu, u32 adr, u64 val);
