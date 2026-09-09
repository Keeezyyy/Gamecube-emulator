#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/emit.h"
#include "cpu/translation/translation_core_macros.h"
#include "translation.h"
#include "translation_core_defines.h"
#include <_abort.h>
#include <stdbool.h>
#include <stdio.h>

#define CORRECT_ENDIAN(x) (_endian32(x, is_little_endian))

static inline uint32_t _endian32(uint32_t x, bool is_little_endian)
{
    if (!is_little_endian) {
        return ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) | ((x & 0x00FF0000) >> 8) |
               ((x & 0xFF000000) >> 24);
    }
    return x;
}
static inline u8 _get_op_from_instruction(u32 i)
{
    return i & 0x3f;
}
static inline u32 _get_ext_from_instruction(u32 instruction, u8 start, u8 end)
{
    u32 mask = (1u << (end - start + 1)) - 1u;
    return (instruction >> start) & mask;
}

// NOTE: a instuction, that flips the 31st bit in the msr register is a therminating instruction
// !!!!
static HostArchOutput _translate_instruction(u32 instruction)
{
    EmitedBlock buffer[STATIC_CODE_BLOCK_BUFFER] = {0};
    HostArchOutput out;
    out.emmited_blocks_ptr = buffer;

    u8 op = _get_op_from_instruction(instruction);
    printf("instuction : 0x%08x\n", instruction);
    printf("op : %d\n", op);

    switch (op) {
    case OPC_STMW: {

        break;
    }
    case OPC_LFD: {
        emit_cbz(&out.emmited_blocks_ptr[0], &out.emmited_blocks_ptr[0].size,
                 _get_ext_from_instruction(instruction, 11, 15), );
        break;
    }
    }
}

TranslationBlock *tb_translate(CPU *cpu, CpuMode cpu_mode)
{
    u32 pc = cpu->state.pc;
    u32 *current_instruction = cpu->bus->read(cpu->bus, pc);

    bool is_little_endian = BIT_CHECK(cpu_mode.val, 31);

    HostArchOutput out;
    do {
        out = _translate_instruction(CORRECT_ENDIAN(*current_instruction++));

    } while (!out.is_terminating_instruction);
}
