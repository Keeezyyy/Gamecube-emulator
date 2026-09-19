#pragma once
#include "bus/bus.h"
#include "bus/ipl.h"
#include "cpu/cpu_types.h"

#define INTERRUPT_SOURCE_PI_ERROR 0
#define INTERRUPT_SOURCE_RSW 1
#define INTERRUPT_SOURCE_DI 2
#define INTERRUPT_SOURCE_SI 3
#define INTERRUPT_SOURCE_EXI 4
#define INTERRUPT_SOURCE_AI 5
#define INTERRUPT_SOURCE_DSP 6
#define INTERRUPT_SOURCE_MEM 7
#define INTERRUPT_SOURCE_VI 8
#define INTERRUPT_SOURCE_PE_TOKEN 9
#define INTERRUPT_SOURCE_PE_FINISH 10
#define INTERRUPT_SOURCE_CP 11
#define INTERRUPT_SOURCE_DEBUG 12
#define INTERRUPT_SOURCE_HSP 13
#define INTERRUPT_SOURCE_IPC 14
#define INTERRUPT_SOURCE_RSWST 16

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

void pi_recieve_gx_gather_piper(CPU *cpu, u32 *buffer);

void pi_activate_external_interrupt(CPU *cpu, u8 interrupt_source);
void pi_deactivate_external_interrupt(CPU *cpu, u8 interrupt_source);
void pi_update_interrupts(CPU *cpu);
