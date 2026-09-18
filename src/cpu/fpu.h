#pragma once

#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include <stdbool.h>

#define FPSCR_FX (1u << 31)
#define FPSCR_FEX (1u << 30)
#define FPSCR_VX (1u << 29)
#define FPSCR_OX (1u << 28)
#define FPSCR_UX (1u << 27)
#define FPSCR_ZX (1u << 26)
#define FPSCR_XX (1u << 25)
#define FPSCR_VXSNAN (1u << 24)
#define FPSCR_VXISI (1u << 23)
#define FPSCR_VXIDI (1u << 22)
#define FPSCR_VXZDZ (1u << 21)
#define FPSCR_VXIMZ (1u << 20)
#define FPSCR_VXVC (1u << 19)
#define FPSCR_FR (1u << 18)
#define FPSCR_FI (1u << 17)
#define FPSCR_FPRF_SHIFT 12
#define FPSCR_FPRF_MASK (0x1fu << FPSCR_FPRF_SHIFT)
#define FPSCR_FPCC_MASK (0xfu << FPSCR_FPRF_SHIFT)
#define FPSCR_VXSOFT (1u << 10)
#define FPSCR_VXSQRT (1u << 9)
#define FPSCR_VXCVI (1u << 8)
#define FPSCR_VE (1u << 7)
#define FPSCR_OE (1u << 6)
#define FPSCR_UE (1u << 5)
#define FPSCR_ZE (1u << 4)
#define FPSCR_XE (1u << 3)
#define FPSCR_NI (1u << 2)
#define FPSCR_RN_MASK 3u

#define FPU_ARG_A 0
#define FPU_ARG_B 2
#define FPU_ARG_C 4
#define FPU_ARG_D 6

const char *fpu_mnemonic(u32 insn);

void fpu_execute(CPU *cpu, u32 insn, bool pse);
