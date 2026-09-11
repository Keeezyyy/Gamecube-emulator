#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/emit.h"
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
    return (i >> 26) & 0x3f;
}
static inline u32 _get_field(u32 insn, u8 start, u8 end)
{
    u32 width = end - start + 1;
    return (insn >> (31 - end)) & ((1u << width) - 1u);
}
static inline bool _get_bit(u32 insn, u8 n)
{
    return (insn >> (31 - n)) & 1u;
}

static inline void _print_instruction(EmitedBlock *b)
{

    char hex[512] = {0};
    char spaced[16 * 512] = {0};

    u32 instructions_pushed = 0;
    for (int i = 0; i < b->num_of_host_instrucion_blocks; i++) {
        for (int j = 0; j < b->host_instruction_buffer[i].num_of_instructions; j++) {
            u32 v = b->host_instruction_buffer[i].host_code_buffer[j];
            printf("v : 0x%08x\n", v);
            snprintf(hex + instructions_pushed * 8, sizeof(hex) - i * 8, "%08x",
                     _endian32(v, false));
            instructions_pushed++;
        }
    }

    size_t n = 0;
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2)
        n += snprintf(spaced + n, sizeof(spaced) - n, "0x%c%c ", hex[i], hex[i + 1]);

    printf("---------------------------------------------------------------------------------------"
           "---------------------------------------------------------------------------------------"
           "------------------------------------\n");
    printf("disasm %s\n", hex);
    fflush(stdout);

    FILE *p = popen("xcrun llvm-mc --disassemble -triple=arm64", "w");
    if (!p) {
        perror("popen");
        return;
    }
    fprintf(p, "%s\n", spaced);
    pclose(p);

    printf("---------------------------------------------------------------------------------------"
           "---------------------------------------------------------------------------------------"
           "------------------------------------\n");
}

// NOTE: a instuction, that flips the 31st bit in the msr register is a therminating instruction
// !!!!
static HostArchOutput _translate_instruction(u32 instruction, CPU *cpu, u32 *pc_after_instruction,
                                             u32 *pc_buffer, u32 pc_buffer_counter)
{
    EmitedBlock emitted_block = {0};

    u32 host_instruction_counter = 0;
    const u8 op = _get_op_from_instruction(instruction);
    printf("instuction : 0x%08x\n", instruction);
    printf("op : %d\n", op);

    // NOTE: the after_instruction_pc must be set in every case
    switch (op) {
    case OPC_BX: {
        u32 LI = _get_field(instruction, 6, 29) << 2;
        if (!_get_bit(instruction, 30)) {
            LI += pc_buffer[pc_buffer_counter];
        }
        if (BIT_CHECK(instruction, 31)) {

            LOAD_64_BIT_IMM(EMIT_STANDART_PARAMS, 0, (u64)&cpu->special_purpose_registers.lr,
                            HOST_R17) // save pointer to LR Register in HOST_R17

            LOAD_64_BIT_IMM(EMIT_STANDART_PARAMS, 1, (u64)pc_buffer[pc_buffer_counter],
                            HOST_R18) // save current pc in r18

            emit_ldr(EMIT_STANDART_PARAMS, 2, HOST_R18, HOST_R18, true, STR_POST_INDEX,
                     4); // save the pc in r18 and adds 4 after

            emit_str(EMIT_STANDART_PARAMS, 3, HOST_R18, HOST_R17, false, STR_POST_INDEX, 0x0);
        }
        *pc_after_instruction = LI;

        break;
    }
    case OPC_ADDIS: {
        printf("!!!!!! val : 0x%08x\n", _get_field(instruction, 16, 31));
        emit_cbz_cbnz(EMIT_STANDART_PARAMS, 0, _get_field(instruction, 11, 15) + 32, false, false,
                      4); // branch to the host instruction block with the id 3

        // primary if  block
        GUEST_LOAD_32_BIT_IMM(EMIT_STANDART_PARAMS, 1, _get_field(instruction, 16, 31) << 16,
                              _get_field(instruction, 6, 10) + 32)

        emit_b(EMIT_STANDART_PARAMS, 4, 7);

        // else  block

        emit_mov(EMIT_STANDART_PARAMS, 4, HOST_R19, _get_field(instruction, 11, 15) + 32, false);
        GUEST_LOAD_32_BIT_IMM(EMIT_STANDART_PARAMS, 1, _get_field(instruction, 16, 31) << 16,
                              HOST_R20)

        emit_add(EMIT_STANDART_PARAMS, 6, _get_field(instruction, 6, 10) + 32, HOST_R19, HOST_R20,
                 false, ADD_SHIFT_LSR, 0);
        break;
    }
    }

    _print_instruction(&emitted_block);
}

TranslationBlock *tb_translate(CPU *cpu, CpuMode cpu_mode)
{
    // u32 pc = cpu->state.pc;
    u32 pc = 0xfff00198;

    bool is_little_endian = BIT_CHECK(cpu_mode.val, 31);

    u32 pc_during_instruction = pc;
    u32 pc_after_instruction = pc;

    u32 pc_buffer[MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK];
    u16 pc_buffer_counter = 0;
    HostArchOutput out;
    do {

        pc_during_instruction = pc_after_instruction;
        pc_buffer[pc_buffer_counter] = pc_during_instruction;
        u32 *current_instruction = cpu->bus->read(cpu->bus, pc_during_instruction);
        out = _translate_instruction(CORRECT_ENDIAN(*current_instruction), cpu,
                                     &pc_after_instruction, pc_buffer, pc_buffer_counter);

        printf("next pc : 0x%08x\n", pc_after_instruction);
        abort();
        pc_buffer_counter++;

    } while (!out.is_terminating_instruction);
}
