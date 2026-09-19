#pragma once

#include "cpu/cpu_types.h"

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

u64 exi_read(CPU *cpu, u32 adr, u32 size);
void exi_write(CPU *cpu, u32 adr, u64 val, u32 size);

void init_exi(void);
u32 exi_get_csr(u8 channel);
