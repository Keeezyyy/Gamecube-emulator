#pragma once

#include "core/config/config.h"
#include "cpu/cpu_types.h"

#define SI_CMD_STATUS_REQ 0x00

#define SI_TYPE_GC_CONTROLLER 0x09000000
#define SI_TYPE_NOT_CONNECTED 0x00000008

#define SI_POLL_ADR 0xCC006430
#define SI_CR_ADR 0xCC006434
#define SI_SISR_ADR 0xCC006438
#define SI_EXILK_ADR 0xCC00643C

#define SI_BUFFER_START_ADR 0xCC006480
#define SI_BUFFER_END_ADR 0xCC0064FF

#define SI_CMD_GET_STATUS_ID 0x00

#define NUM_OF_CHANNELS 4

typedef struct {
    u32 OUTBUF;
    u32 INBUFH;
    u32 INBUFL;
} PACKED SIChannel;

typedef struct {
    SIChannel channel[NUM_OF_CHANNELS];
    u32 SIPOLL;
    u32 SICOMCSR;
    u32 SISR;
    u32 SIEXILK;
    u32 _reserved[16];
    u32 SIIOBUF[32];
} PACKED SIRegisters;

void si_write(CPU *cpu, u32 adr, u64 val, u32 size);
u64 si_read(CPU *cpu, u32 adr, u32 size);
u32 si_get_comcsr(void);

void si_vblank_trigger(void);
