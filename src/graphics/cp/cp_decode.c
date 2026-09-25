#include "bus/bus.h"
#include "core/config/config.h"
#include "cp.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/render/pipeline.h"
#include "graphics/gpu/vertex/primitive.h"
#include "graphics/pe.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <_abort.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <GLFW/glfw3.h>

#ifdef RENDER_TEST_RAYLIB
#include <raylib.h>
#endif

static u32 bp_regs[256];
static u32 cp_regs[256]; // ends with 0xBF
static u32 xf_regs[0x1057];

u32 *get_xf_register_pointer(void)
{

#ifdef SOFTWARE_TRANSFORM
    return software_backend_get_xf_buffer();
#endif
}
u32 *get_bp_register_pointer(void)
{
    return bp_regs;
}
u32 *get_cp_register_pointer(void)
{
    return cp_regs;
}

static u32 bp_mask = 0xFFFFFF;
static void load_bp_reg(CPU *cpu, u32 cmd)
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

    GPU_PRINT("BP LOAD [0x%02x] = 0x%06x\n", reg, new_val);

    if (reg == 0x45 || reg == 0x47 || reg == 0x48 || reg == 0x52 || reg == 0x55 || reg == 0x56 ||
        reg == 0x57 || reg == 0x63 || reg == 0x64 || reg == 0x65 || reg == 0x66) {
        switch (reg) {
        case BP_SET_DRAW_DONE: {
            pe_set_interrupt(cpu, PE_INTERRUPT_FINISH);
            GPU_PRINT("[GPU] : GX_DawDone\n");
            GPU_PRINT("----------------------------------------------------------------------------"
                      "-----\n");
            // trigger for render

            // cpy_reg_state(bp_regs, cp_regs, xf_regs);
            break;
        }
        case BP_SET_PE_TOKEN: {
            assert(!"pe token");
            break;
        }
        case 0x52: {
            // 0x52 TRIGGER_EFB_COPY GX_CopyDisp/CopyTex startet Kopie (+Clear)

            GPU_PRINT("BP reg 0x52 write\n");
#ifdef RENDER_TEST_RAYLIB
            if ((new_val >> 14) & 1) {
                EndDrawing();
                BeginDrawing();
                ClearBackground(BLACK);
            }
#endif
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

static u32 _get_fifo_end(GXFifoRegs *command_processor_registers)
{
    return atomic_load(&command_processor_registers->fifo_markers.FIFO_END);
}
static u32 _get_fifo_base(GXFifoRegs *command_processor_registers)
{
    return atomic_load(&command_processor_registers->fifo_markers.FIFO_BASE);
}

static void load_cp_reg(u8 reg_num, u32 val)
{
    assert(reg_num <= 0xBF);
    cp_regs[reg_num] = val;

    GPU_PRINT("CP LOAD [0x%02x] = 0x%06x\n", reg_num, val);
}

u64 read_stream(CPU *cpu, GXFifoRegs *command_processor_registers, u8 **stream, u8 size)
{
    u64 val = 0;

    for (u8 i = 0; i < size; i++) {
        if (command_processor_registers &&
            *stream > &cpu->bus->ram[_get_fifo_end(command_processor_registers) + 3])
            *stream = &cpu->bus->ram[_get_fifo_base(command_processor_registers)];

        val = (val << 8) | *(*stream)++;
    }

    return val;
}

static u32 _comp_size(u32 fmt)
{
    return fmt == DATA_TYPE_F32 ? 4 : fmt > DATA_TYPE_S8 ? 2 : 1;
}

static u32 _attr_size(u32 type, u32 direct)
{
    return type == 0 ? 0 : type == 1 ? direct : type - 1;
}

u32 get_size_of_vertex(u32 VCD_HI, u32 VCD_LO, u32 VAT_A, u32 VAT_B, u32 VAT_C)
{
    static const u8 elem_bit[8] = {21, 32, 41, 50, 59, 69, 78, 87};
    static const u32 clr_size[8] = {2, 3, 4, 2, 3, 4, 0, 0};

    const u32 VAT[3] = {VAT_A, VAT_B, VAT_C};
    const u32 nrm_type = (VCD_LO >> 11) & 3;
    const u32 nrm_elements = (VAT_A >> 9) & 1;

    u32 size = (u32)__builtin_popcount(VCD_LO & 0x1FF);

    size += _attr_size((VCD_LO >> 9) & 3, (2 + (VAT_A & 1)) * _comp_size((VAT_A >> 1) & 7));

    size += _attr_size(nrm_type, (nrm_elements ? 9 : 3) * _comp_size((VAT_A >> 10) & 7));
    if (nrm_type > 1 && nrm_elements && (VAT_A >> 31))
        size += 2 * (nrm_type - 1);

    size += _attr_size((VCD_LO >> 13) & 3, clr_size[(VAT_A >> 14) & 7]);
    size += _attr_size((VCD_LO >> 15) & 3, clr_size[(VAT_A >> 18) & 7]);

    for (u8 i = 0; i < 8; i++) {
        const u32 w = VAT[elem_bit[i] >> 5] >> (elem_bit[i] & 31);
        size += _attr_size((VCD_HI >> (i * 2)) & 3, (1 + (w & 1)) * _comp_size((w >> 1) & 7));
    }

    return size;
}

static void load_primitive(CPU *cpu, GXFifoRegs *command_processor_registers,
                           u8 primitive_info_byte, const u16 vertex_count, u8 **stream)
{
    const u8 vat_index = primitive_info_byte & 0x7;
    const u8 primitive_type = (primitive_info_byte >> 3) & 0x7;

    GPU_PRINT("PRIMITIVE: type :  0x%02x, index :  0x%02x , length  0x%04x\n", primitive_type,
              vat_index, vertex_count);

    init_vertex_loader(cpu, command_processor_registers);

    // TODO: find better alternative maybe gloabl dynamic array
    Vertex buffer[U16_MAX];
    for (u16 i = 0; i < vertex_count; i++) {
        Vertex v = parse_vertex_from_stream(
            cpu, cp_regs[0x60], cp_regs[0x50], cp_regs[0x70 + vat_index], cp_regs[0x80 + vat_index],
            cp_regs[0x90 + vat_index], cp_regs, stream, primitive_type, vertex_count);

        if (buffer[i].pm.posMatId == 0xFF) {
            // continue;
        }
        buffer[i] = v;
    }

    Primitive p = {buffer, vertex_count, primitive_type};
    load_vertex_into_pipeline(cpu, p);
}

static void load_xf_reg(u16 adr, u16 n, const u32 *values)
{

    GPU_PRINT("XF: adr:  0x%04x n: 0x%04x\n", adr, n);

    assert(adr <= 0x1057);
    assert(n <= 16);

    for (int i = 0; i < n; i++) {
        // xf_regs[adr + i] = values[i];
#ifdef SOFTWARE_TRANSFORM
        software_backend_write_to_xf_reg(adr + i, values[i]);
#endif
    }
}

static void call_display_list(CPU *cpu, GXFifoRegs *command_processor_registers, const u32 adr,
                              const u32 size)
{

    u8 *stream = (u8 *)&cpu->bus->ram[adr];
    while (stream < (u8 *)&cpu->bus->ram[adr + size]) {
        const u8 op = stream[0];
        u8 *before = stream;
        execute_command(cpu, command_processor_registers, op, &stream, false);

        if (stream == before)
            return;
    }
}

void execute_command(CPU *cpu, GXFifoRegs *command_processor_registers, const u8 op,
                     u8 **stream_ptr, bool decrease_rw_distance)
{
    GXFifoRegs *fifo = decrease_rw_distance ? command_processor_registers : NULL;
    u8 *stream = *stream_ptr;

    read_stream(cpu, fifo, &stream, 1);

    u32 rw_d = atomic_load(&command_processor_registers->fifo_markers.RW_DISTANCE);

    switch (op) {
    case OPCODE_NOP:
    case OPCODE_INVL_VC:
        break;
    case OPCODE_LOAD_BP_REG: {
        if (rw_d < OPCODE_LOAD_BP_REG_LENGTH)
            return;

        load_bp_reg(cpu, (u32)read_stream(cpu, fifo, &stream, 4));
        break;
    }
    case OPCODE_LOAD_CP_REG: {
        if (rw_d < OPCODE_LOAD_CP_REG_LENGTH)
            return;

        const u8 reg = (u8)read_stream(cpu, fifo, &stream, 1);
        load_cp_reg(reg, (u32)read_stream(cpu, fifo, &stream, 4));
        break;
    }
    case OPCODE_LOAD_XF_REG: {
        const u32 head = (u32)read_stream(cpu, fifo, &stream, 4);
        const u16 n = ((head >> 16) & 0xF) + 1;

        if (rw_d < (5 + (n * 4)))
            return;

        u32 values[16];
        for (u16 i = 0; i < n; i++)
            values[i] = (u32)read_stream(cpu, fifo, &stream, 4);

        load_xf_reg(head & 0xFFFF, n, values);
        break;
    }
    case OPCODE_CALL_DL: {
        if (rw_d < OPCODE_CALL_DL_LENGTH)
            return;

        const u32 adr = (u32)read_stream(cpu, fifo, &stream, 4) & 0x03FFFFE0;
        const u32 size = (u32)read_stream(cpu, fifo, &stream, 4) & 0x03FFFFE0;

        call_display_list(cpu, command_processor_registers, adr, size);
        break;
    }
    default: {
        if (op < OPCODE_PRIMITIVE_START || op > OPCODE_PRIMITIVE_END) {
            GPU_PRINT("opcode : 0x%02x\n", op);
            assert(!"gpu opcode not implemented\n");
            return;
        }

        const u16 vertex_count = (u16)read_stream(cpu, fifo, &stream, 2);
        const u8 vat_index = op & 0x7;
        const u32 vertex_size =
            get_size_of_vertex(cp_regs[0x60], cp_regs[0x50], cp_regs[0x70 + vat_index],
                               cp_regs[0x80 + vat_index], cp_regs[0x90 + vat_index]);

        if (rw_d < (vertex_size * vertex_count) + 3)
            return;

        load_primitive(cpu, fifo, op, vertex_count, &stream);
        break;
    }
    }

    u32 len = (u32)(stream - *stream_ptr);
    if (fifo && stream < *stream_ptr)
        len += _get_fifo_end(command_processor_registers) + 4 -
               _get_fifo_base(command_processor_registers);

    *stream_ptr = stream;

    if (decrease_rw_distance) {
        atomic_store(&command_processor_registers->fifo_markers.READ_POINTER,
                     (u32)(stream - cpu->bus->ram));
        atomic_fetch_sub(&command_processor_registers->fifo_markers.RW_DISTANCE, len);
    }
}

void decode_data_stream(CPU *cpu, GXFifoRegs *command_processor_registers)
{
    u8 *stream =
        (u8 *)&cpu->bus->ram[atomic_load(&command_processor_registers->fifo_markers.READ_POINTER)];

    while (atomic_load(&command_processor_registers->fifo_markers.RW_DISTANCE) != 0) {
        if (stream > &cpu->bus->ram[_get_fifo_end(command_processor_registers) + 3])
            stream = &cpu->bus->ram[_get_fifo_base(command_processor_registers)];
        const u8 op = stream[0];
        u8 *before = stream;
        execute_command(cpu, command_processor_registers, op, &stream, true);

        if (stream == before)
            return;
    }
}
