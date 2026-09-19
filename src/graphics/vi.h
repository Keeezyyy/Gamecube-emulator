#pragma once

#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include <stdint.h>

typedef struct {
    volatile uint16_t VTR; // +0x00 Vertical Timing
    volatile uint16_t DCR; // +0x02 Display Configuration

    volatile uint32_t HTR0; // +0x04 Horizontal Timing 0
    volatile uint32_t HTR1; // +0x08 Horizontal Timing 1

    volatile uint32_t VTO; // +0x0C Vertical Timing Odd
    volatile uint32_t VTE; // +0x10 Vertical Timing Even

    volatile uint32_t BBOI; // +0x14 Burst Blanking Odd
    volatile uint32_t BBEI; // +0x18 Burst Blanking Even

    volatile uint32_t TFBL; // +0x1C Top Field Base Left
    volatile uint32_t TFBR; // +0x20 Top Field Base Right (3D)
    volatile uint32_t BFBL; // +0x24 Bottom Field Base Left
    volatile uint32_t BFBR; // +0x28 Bottom Field Base Right (3D)

    volatile uint16_t DPV; // +0x2C Display Position Vertical (r)
    volatile uint16_t DPH; // +0x2E Display Position Horizontal (r)

    volatile uint32_t DI0; // +0x30 Display Interrupt 0
    volatile uint32_t DI1; // +0x34 Display Interrupt 1
    volatile uint32_t DI2; // +0x38 Display Interrupt 2
    volatile uint32_t DI3; // +0x3C Display Interrupt 3

    volatile uint32_t DL0; // +0x40 Display Latch 0
    volatile uint32_t DL1; // +0x44 Display Latch 1

    volatile uint16_t HSW; // +0x48 Scaling Width / Picture Config
    volatile uint16_t HSR; // +0x4A Horizontal Scaling

    volatile uint32_t FCT[7]; // +0x4C ... +0x64 Filterkoeffizienten

    volatile uint32_t AA_UNKNOWN; // +0x68 Unbekanntes AA-Register

    volatile uint16_t VICLK; // +0x6C Video Clock
    volatile uint16_t VISEL; // +0x6E DTV-Status

    volatile uint16_t FBWIDTH; // +0x70 Horizontal Stepping
    volatile uint16_t HBE;     // +0x72 Border HBE
    volatile uint16_t HBS;     // +0x74 Border HBS

} PACKED DisplayRegisters;

void vi_write(CPU *cpu, u32 adr, u32 val, u32 size);
u32 vi_read(CPU *cpu, u32 adr, u32 size);
void vi_init(void);
u32 vi_get_display_interrupt(u8 index);
