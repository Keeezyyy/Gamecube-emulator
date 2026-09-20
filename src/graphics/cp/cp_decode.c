#include "cp.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

static u32 bp_regs[256];

static u32 bp_mask;
static void load_bp_reg(u32 cmd)
{

    u8 reg = cmd >> 24;
    u32 value = cmd & 0xFFFFFF;

    u32 old = bp_regs[reg];
    u32 new_val = (old & ~bp_mask) | (value & bp_mask);

    if (reg != 0xFE)
        bp_mask = 0xFFFFFF;

    bp_regs[reg] = new_val;

    if (reg == 0xFE)
        bp_mask = value;

    printf("BP LOAD [0x%02x] = 0x%06x\n", reg, new_val);

    // trigger
}

void decode_data_stream(CPU *cpu, GXFifoRegs *cp_regs)
{
    u8 *stream = (u8 *)&cpu->bus->ram[cp_regs->READ_POINTER];

    while (stream <= (u8 *)&cpu->bus->ram[cp_regs->WRITE_POINTER]) {
        const u8 op = stream[0];
        switch (op) {
        case OPCODE_NOP: {
            stream += OPCODE_NOP_LENGTH;

            break;
        }
        case OPCODE_LOAD_BP_REG: {
            load_bp_reg(*(u32 *)(stream + 1));
            stream += OPCODE_LOAD_BP_REG_LENGTH;

            break;
        }
        default: {
            printf("opcode : 0x%02x\n", op);
            assert(!"gpu opcode not implemented\n");
        }
        }
    }
}
