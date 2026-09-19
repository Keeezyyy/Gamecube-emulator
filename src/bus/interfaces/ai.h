#pragma once

#include "cpu/cpu_types.h"
#define DSP_CONTROL 0xCC00500A
#define DSP_MAIL_FROM_DSP_HI 0xCC005004
#define DSP_MAIL_FROM_DSP_LO 0xCC005006
#define DSP_AR_INFO 0xCC005012
#define DSP_AR_REFRESH 0xCC00501A
#define DSP_MAIL_TO_DSP_HI 0xCC005000
#define AR_DMA_MMADDR 0xCC005020
#define AR_MODE 0xCC005016

#define ARAM_CAPACITY_IN_MB 16

typedef struct {
    volatile uint32_t AICR;   // 0x00 - Control Register
    volatile uint32_t AIVR;   // 0x04 - Volume: Bit 0-7 left, 8-15 right
    volatile uint32_t AISCNT; // 0x08 - Sample counter
    volatile uint32_t AIIT;   // 0x0C - Interrupt timing
} AudioRegs;

void dsp_write(CPU *cpu, u32 adr, u32 val, u32 size);
u64 dsp_read(CPU *cpu, u32 adr, u32 size);

u64 ai_read(CPU *cpu, u32 adr, u32 size);
void ai_write(CPU *cpu, u32 adr, u64 val, u32 size);
void ai_init(void);
