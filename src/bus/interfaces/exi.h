#pragma once

#include "cpu/cpu_types.h"

typedef struct {
    u64 EXInCSR;
    u64 EXInMAR;
    u64 EXInLENGTH;
    u64 EXInCR;
    u64 EXInDATA;
} PACKED RegistersPerChannel;

#define EXIINTMASK_OFF 0
#define EXIINT_OFF 1
#define TCINTMASK_OFF 2
#define TCINT_OFF 3

#define CLK_OFF 4
#define CLK_MASK (0x7u << CLK_OFF)

#define CS_OFF 7
#define CS_MASK (0x7u << CS_OFF)

#define CS0 (1u << 7)
#define CS1 (1u << 8)
#define CS2 (1u << 9)

#define EXTINTMASK_OFF 10
#define EXTINT_OFF 11
#define EXT_OFF 12
#define ROMDIS_OFF 13

#define EXIINTMASK (1u << EXIINTMASK_OFF)
#define EXIINT (1u << EXIINT_OFF)
#define TCINTMASK (1u << TCINTMASK_OFF)
#define TCINT (1u << TCINT_OFF)

#define EXTINTMASK (1u << EXTINTMASK_OFF)
#define EXTINT (1u << EXTINT_OFF)
#define EXT (1u << EXT_OFF)
#define ROMDIS (1u << ROMDIS_OFF)

#define EXI_CR_TSTART (1u << 0)
#define EXI_CR_DMA (1u << 1)
#define EXI_CR_RW(cr) (((u32)(cr) >> 2) & 0x3)
#define EXI_CR_TLEN(cr) ((((u32)(cr) >> 4) & 0x3) + 1)

#define EXI_RW_READ 0
#define EXI_RW_WRITE 1

u64 exi_read(CPU *cpu, u32 adr, u32 size);
void exi_write(CPU *cpu, u32 adr, u64 val, u32 size);
u32 exi_push_data(CPU *cpu, RegistersPerChannel *r, const u8 *data, u32 length);
u32 exi_pull_data(CPU *cpu, RegistersPerChannel *r, u8 *data, u32 length);

void init_exi(void);
u32 exi_get_csr(u8 channel);
