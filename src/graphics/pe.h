#pragma once

#include "core/config/config.h"
#include "cpu/cpu_types.h"
typedef struct PERegs {
    /* +0x00 */
    volatile uint16_t ZCONF;

    /* +0x02 */
    volatile uint16_t ALPHACONF;

    /* +0x04 */
    volatile uint16_t DSTALPHACONF;

    /* +0x06 */
    volatile uint16_t ALPHAMODE;

    /* +0x08 */
    volatile uint16_t ALPHAREAD;

    /* +0x0A */
    volatile uint16_t CTRL;
} PACKED PERegs;

void pe_write(CPU *cpu, u32 adr, u32 val, u32 size);
u64 pe_read(CPU *cpu, u32 adr, u32 size);
u16 pe_get_ctrl(void);
