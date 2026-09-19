#pragma once

#include "cpu/cpu_types.h"
#define DSP_BASE 0xCC005000
#define DSP_MAIL_TO_DSP_HI 0xCC005000
#define DSP_MAIL_TO_DSP_LO 0xCC005002
#define DSP_MAIL_FROM_DSP_HI 0xCC005004
#define DSP_MAIL_FROM_DSP_LO 0xCC005006
#define DSP_CONTROL 0xCC00500A
#define DSP_INTERRUPT_CONTROL 0xCC005010
#define DSP_AR_INFO 0xCC005012
#define DSP_AR_MODE 0xCC005016
#define DSP_AR_REFRESH 0xCC00501A
#define DSP_AR_DMA_MMADDR_H 0xCC005020
#define DSP_AR_DMA_MMADDR_L 0xCC005022
#define DSP_AR_DMA_ARADDR_H 0xCC005024
#define DSP_AR_DMA_ARADDR_L 0xCC005026
#define DSP_AR_DMA_CNT_H 0xCC005028
#define DSP_AR_DMA_CNT_L 0xCC00502A
#define DSP_AUDIO_DMA_START_H 0xCC005030
#define DSP_AUDIO_DMA_START_L 0xCC005032
#define DSP_AUDIO_DMA_BLOCKS_LENGTH 0xCC005034
#define DSP_AUDIO_DMA_CONTROL_LEN 0xCC005036
#define DSP_AUDIO_DMA_BLOCKS_LEFT 0xCC00503A

#define ARAM_CAPACITY_IN_MB 16

typedef struct {
    volatile uint16_t MAIL_TO_DSP_HI;
    volatile uint16_t MAIL_TO_DSP_LO;
    volatile uint16_t MAIL_FROM_DSP_HI;
    volatile uint16_t MAIL_FROM_DSP_LO;
    volatile uint16_t PAD0;
    volatile uint16_t CSR;
    volatile uint16_t PAD1[2];
    volatile uint16_t INTERRUPT_CONTROL;
    volatile uint16_t AR_INFO;
    volatile uint16_t PAD2;
    volatile uint16_t AR_MODE;
    volatile uint16_t PAD3;
    volatile uint16_t AR_REFRESH;
    volatile uint16_t PAD4[2];
    volatile uint16_t AR_DMA_MMADDR_H;
    volatile uint16_t AR_DMA_MMADDR_L;
    volatile uint16_t AR_DMA_ARADDR_H;
    volatile uint16_t AR_DMA_ARADDR_L;
    volatile uint16_t AR_DMA_CNT_H;
    volatile uint16_t AR_DMA_CNT_L;
    volatile uint16_t PAD5[2];
    volatile uint16_t AUDIO_DMA_START_H;
    volatile uint16_t AUDIO_DMA_START_L;
    volatile uint16_t AUDIO_DMA_BLOCKS_LENGTH;
    volatile uint16_t AUDIO_DMA_CONTROL_LEN;
    volatile uint16_t PAD6;
    volatile uint16_t AUDIO_DMA_BLOCKS_LEFT;
} PACKED DSPRegisters;

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
