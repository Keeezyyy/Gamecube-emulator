#include "cp.h"
#include <_abort.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

static u32 bp_regs[256];

static u32 bp_mask = 0xFFFFFF;
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

static u32 cp_regs[256]; // ends with 0xBF
static void load_cp_reg(u8 reg_num, u32 val)
{
    assert(reg_num <= 0xBF);
    cp_regs[reg_num] = val;

    printf("CP LOAD [0x%02x] = 0x%06x\n", reg_num, val);
}

static u16 load_primitive(u8 primitive_info_byte, const u16 vertex_count,
                          const u32 *stream) // returns total length of command
{
    const u8 vat_index = primitive_info_byte & 0x7;
    const u8 primitive_type = (primitive_info_byte >> 3) & 0x7;

    printf("PRIMITIVE: type :  0x%02x, index :  0x%02x , length  0x%04x\n", primitive_type,
           vat_index, vertex_count);
    abort();
}

static void add_len_to_regs(GXFifoRegs *command_processor_registers, u8 **stream, u32 len)
{
    *stream += len;
    command_processor_registers->READ_POINTER += len;
    command_processor_registers->RW_DISTANCE -= len;
}

void decode_data_stream(CPU *cpu, GXFifoRegs *command_processor_registers)
{
    u8 *stream = (u8 *)&cpu->bus->ram[command_processor_registers->READ_POINTER];
    printf("@%08x: %02x %02x %02x %02x %02x %02x\n", (u32)(stream - (u8 *)cpu->bus->ram), stream[0],
           stream[1], stream[2], stream[3], stream[4], stream[5]);

    while (stream <= (u8 *)&cpu->bus->ram[command_processor_registers->WRITE_POINTER]) {
        const u8 op = stream[0];
        switch (op) {
        case OPCODE_NOP: {
            add_len_to_regs(command_processor_registers, &stream, OPCODE_NOP_LENGTH);

            break;
        }
        case OPCODE_LOAD_BP_REG: {
            load_bp_reg(__builtin_bswap32(*(u32 *)(stream + 1)));
            add_len_to_regs(command_processor_registers, &stream, OPCODE_LOAD_BP_REG_LENGTH);

            break;
        }
        case OPCODE_LOAD_CP_REG: {
            load_cp_reg(*(stream + 1), __builtin_bswap32(*(u32 *)(stream + 2)));
            add_len_to_regs(command_processor_registers, &stream, OPCODE_LOAD_CP_REG_LENGTH);

            break;
        }
        default: {
            if (op >= OPCODE_PRIMITIVE_START && op <= OPCODE_PRIMITIVE_END) {
                u32 len = load_primitive(op, *(u16 *)(stream + 1), (u32 *)(stream + 3));
                add_len_to_regs(command_processor_registers, &stream, len);

            } else {
                printf("opcode : 0x%02x\n", op);
                assert(!"gpu opcode not implemented\n");
            }
        }
        }
    }
}
