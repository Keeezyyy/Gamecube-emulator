#pragma once

#include "core/config/config.h"
#include "cpu/cpu_types.h"
typedef struct PERegs {
    volatile uint16_t ZCONF;

    volatile uint16_t ALPHACONF;

    volatile uint16_t DSTALPHACONF;

    volatile uint16_t ALPHAMODE;

    volatile uint16_t ALPHAREAD;

    volatile uint16_t CTRL;
} PACKED PERegs;

void pe_write(CPU *cpu, u32 adr, u32 val, u32 size);
u64 pe_read(CPU *cpu, u32 adr, u32 size);
u16 pe_get_ctrl(void);

#define PE_INTERRUPT_TOKEN 0
#define PE_INTERRUPT_FINISH 1
void pe_set_interrupt(CPU *cpu);

void pe_set_token(u32 t, CPU *cpu, bool set_interrupt);
