#include "backend.h"
#include "bus/bus.h"
#include "./instructions.h"
#include <assert.h>

#include <oaknut_c.h>

static translate_fn no_extended_opcode_instruction_routines[62] = {
    [14] = 0x0,
};

void init_instruction_routine_array(void)
{
    no_extended_opcode_instruction_routines[14] = translate_addi_func;
}

static inline u8 _get_op_from_instruction(u32 i)
{
    return (i >> 26) & 0x3f;
}

u32 *backend_translate_guest_instructions(const u32 instruction, CPU *cpu,
                                          u32 *pc_after_instruction, u32 *pc_buffer,
                                          u32 *host_block_offsets, u32 pc_buffer_counter,
                                          u32 *code_buffer, u32 *code_buffer_end,
                                          u8 *termination_type, u8 *tb_type)
{
    const u8 opcode = _get_op_from_instruction(instruction);

    u32 *(*func)(TranslateFuncContext ctx);

    u32 reg_usage = 0;

    switch (opcode) {
    case 63:
    case 59:
    case 31:
    case 19:
    case 4:
    default:
        func = no_extended_opcode_instruction_routines[opcode];
        return func((TranslateFuncContext){instruction, cpu, pc_after_instruction, pc_buffer,
                                           host_block_offsets, pc_buffer_counter, code_buffer,
                                           code_buffer_end, termination_type, tb_type,
                                           .reg_usage_bit_map = &reg_usage});
    }
}
