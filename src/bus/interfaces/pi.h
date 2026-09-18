#pragma once

#include "cpu/cpu_types.h"
typedef struct PIRegs {

    /* +0x0C */
    volatile uint32_t PI_FIFO_BASE;

    /* +0x10 */
    volatile uint32_t PI_FIFO_END;

    /* +0x14 */
    volatile uint32_t PI_FIFO_WPTR;

    /* +0x18 */
    volatile uint32_t PI_FIFO_RESET; // w

    /* +0x1C */
    volatile uint32_t PI_ERROR_CAUSE;

    /* +0x20 */
    volatile uint32_t PI_ERROR_ADDRESS; // r

    /* +0x24 */
    volatile uint32_t PI_RESET_CODE;

    /* +0x28 */
    volatile uint32_t PI_UNKNOWN;

    /* +0x2C */
    volatile uint32_t PI_FLIPPER_REV; // r

    /* +0x30 */
    volatile uint32_t PI_FLIPPER_BUS_STRENGTH;
} PIRegs;
u64 pi_read(CPU *cpu, u32 adr, u32 size);

void pi_write(CPU *cpu, u32 adr, u64 val, u32 size);
