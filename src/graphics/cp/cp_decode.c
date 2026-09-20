#include "cp.h"
#include "graphics/vertex/vertex_loader.h"
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

    if (reg == 0x45 || reg == 0x47 || reg == 0x48 || reg == 0x52 || reg == 0x55 || reg == 0x56 ||
        reg == 0x57 || reg == 0x63 || reg == 0x64 || reg == 0x65 || reg == 0x66) {
        switch (reg) {
        case 0x52: {
            // 0x52 TRIGGER_EFB_COPY GX_CopyDisp/CopyTex startet Kopie (+Clear)
            break;
        }
        case 0x55:
        case 0x56: {
            // activate bbox
            // TODO: if acutally rendering use these bbox values
            /*
                int off = (reg == 0x55) ? 0 : 2;
                bbox[off + 0] = value & 0x3FF;
                bbox[off + 1] = (value >> 10) & 0x3FF;
                bbox_active = true;

            */
            break;
        }
        case 0x66: {
            // cache invalidieren (ignore)
            break;
        }
        default:
            assert(!"bp reg trigger\n");
        }
    } // trigger
}

static u32 cp_regs[256]; // ends with 0xBF
static void load_cp_reg(u8 reg_num, u32 val)
{
    assert(reg_num <= 0xBF);
    cp_regs[reg_num] = val;

    printf("CP LOAD [0x%02x] = 0x%06x\n", reg_num, val);
}

u64 read_stream(u32 **stream, u8 size)
{
    switch (size) {
    case 1: {
        u8 val = ((u8 *)*stream)[0];
        *stream = (u32 *)(&((u8 *)*stream)[1]);

        return (u64)(val);
    }
    case 2: {

        u16 val = ((u16 *)*stream)[0];
        *stream = (u32 *)(&((u16 *)*stream)[1]);

        return (u64)__builtin_bswap16(val);
    }
    case 4: {

        u32 val = ((u32 *)*stream)[0];
        *stream = (u32 *)(&((u32 *)*stream)[1]);

        return (u64)__builtin_bswap32(val);
    }
    default:
        assert(!"u64 reads from steram\n");
    }
}

// returns 0 if data length is too short for data requiered
static u16 load_primitive(CPU *cpu, u8 primitive_info_byte, const u16 vertex_count,
                          u32 *stream) // returns total length of command
{
    const u8 vat_index = primitive_info_byte & 0x7;
    const u8 primitive_type = primitive_info_byte & ~0x7;

    printf("PRIMITIVE: type :  0x%02x, index :  0x%02x , length  0x%04x\n", primitive_type,
           vat_index, vertex_count);

    // pos data

    u32 VCD = (cp_regs[0x60] << 16) | (cp_regs[0x60] & 0xFFFF);

    u32 VAT_A = cp_regs[0x70 + vat_index];
    u32 VAT_B = cp_regs[0x80 + vat_index];
    u32 VAT_C = cp_regs[0x90 + vat_index];
    Vertex v = parse_vertex_from_stream(cpu, VCD, VAT_A, VAT_B, VAT_C, cp_regs, &stream);
}

static u32 xf_regs[0x1057];

static void load_xf_reg(u16 adr, u16 n, u32 *stream)
{

    printf("XF: adr:  0x%04x n: 0x%04x\n", adr, n);

    assert(adr <= 0x1057);
    assert(n <= 16);

    for (int i = 0; i < n; i++) {
        xf_regs[adr + i] = stream[i];
    }
}

static void add_len_to_regs(GXFifoRegs *command_processor_registers, u8 **stream, u32 len)
{
    *stream += len;
    command_processor_registers->READ_POINTER += len;
    command_processor_registers->RW_DISTANCE -= len;
}

static void call_display_list(CPU *cpu, GXFifoRegs *command_processor_registers, const u32 adr,
                              const u32 size)
{

    u8 *stream = (u8 *)&cpu->bus->ram[adr];
    while (stream < (u8 *)&cpu->bus->ram[adr + size]) {
        const u8 op = stream[0];
        u8 *before = stream;
        execute_command(cpu, command_processor_registers, op, &stream);

        if (stream == before)
            return;
    }
}

void execute_command(CPU *cpu, GXFifoRegs *command_processor_registers, const u8 op,
                     u8 **stream_ptr)
{
    u8 *stream = *stream_ptr;

    switch (op) {
    case OPCODE_NOP:
    case OPCODE_INVL_VC: {

        add_len_to_regs(command_processor_registers, stream_ptr, OPCODE_NOP_LENGTH);

        break;
    }
    case OPCODE_LOAD_BP_REG: {
        if (command_processor_registers->RW_DISTANCE < OPCODE_LOAD_BP_REG_LENGTH)
            return;

        load_bp_reg(__builtin_bswap32(*(u32 *)(stream + 1)));
        add_len_to_regs(command_processor_registers, stream_ptr, OPCODE_LOAD_BP_REG_LENGTH);

        break;
    }
    case OPCODE_LOAD_CP_REG: {
        if (command_processor_registers->RW_DISTANCE < OPCODE_LOAD_CP_REG_LENGTH)
            return;
        load_cp_reg(*(stream + 1), __builtin_bswap32(*(u32 *)(stream + 2)));
        add_len_to_regs(command_processor_registers, stream_ptr, OPCODE_LOAD_CP_REG_LENGTH);

        break;
    }
    case OPCODE_LOAD_XF_REG: {
        const u16 n = ((__builtin_bswap32(*(u32 *)(stream + 1)) >> 16) & 0xF) + 1;

        if (command_processor_registers->RW_DISTANCE < (5 + (n * 4)))
            return;

        load_xf_reg(__builtin_bswap32(*(u32 *)(stream + 1)) & 0xFFFF, n, (u32 *)(stream + 5));

        add_len_to_regs(command_processor_registers, stream_ptr, 5 + (n * 4));

        break;
    }
    case OPCODE_CALL_DL: {
        if (command_processor_registers->RW_DISTANCE < OPCODE_CALL_DL_LENGTH)
            return;
        call_display_list(cpu, command_processor_registers,
                          __builtin_bswap32(*(u32 *)(stream + 1)) & 0x03FFFFE0,
                          __builtin_bswap32(*(u32 *)(stream + 5)) & 0x03FFFFE0);

        assert(!"3245");

        add_len_to_regs(command_processor_registers, stream_ptr, OPCODE_CALL_DL_LENGTH);

        break;
    }
    default: {
        if (op >= OPCODE_PRIMITIVE_START && op <= OPCODE_PRIMITIVE_END) {
            u32 len = load_primitive(cpu, op, __builtin_bswap16(*(u16 *)(stream + 1)),
                                     (u32 *)(stream + 3));
            if (len == 0) {
                return;
            }
            add_len_to_regs(command_processor_registers, stream_ptr, len);

        } else {
            printf("opcode : 0x%02x\n", op);
            assert(!"gpu opcode not implemented\n");
        }
    }
    }
}

void decode_data_stream(CPU *cpu, GXFifoRegs *command_processor_registers)
{
    u8 *stream = (u8 *)&cpu->bus->ram[command_processor_registers->READ_POINTER];
    printf("@%08x: %02x %02x %02x %02x %02x %02x\n", (u32)(stream - (u8 *)cpu->bus->ram), stream[0],
           stream[1], stream[2], stream[3], stream[4], stream[5]);

    while (stream < (u8 *)&cpu->bus->ram[command_processor_registers->WRITE_POINTER]) {
        const u8 op = stream[0];
        u8 *before = stream;
        execute_command(cpu, command_processor_registers, op, &stream);

        if (stream == before)
            return;
    }
}
