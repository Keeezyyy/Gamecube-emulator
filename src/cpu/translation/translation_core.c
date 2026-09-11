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

    for (int i = 0; i < b->num_of_host_instructions; i++) {
        u32 v = b->host_code_buffer[i];
        /*
            printf("------\n");
            printf("for hex editor instruction : 0x%08x\n", _endian32(v, false));
            printf("instruction : 0x%08x\n", _endian32(v, true));
            printf("------\n");
        */

        snprintf(hex + i * 8, sizeof(hex) - i * 8, "%08x", _endian32(v, false));
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
    EmitedBlock emitted_block;

    u8 op = _get_op_from_instruction(instruction);
    printf("instuction : 0x%08x\n", instruction);
    printf("op : %d\n", op);

    // NOTE: the after_instruction_pc must be set in every case
    switch (op) {
    case OPC_BX: {
        u32 LI = _get_field(instruction, 6, 29) << 2;
        if (!_get_bit(instruction, 30)) {
            LI += pc_buffer[pc_buffer_counter]; // convert to host address
        }
        if (BIT_CHECK(instruction, 31)) {

            LOAD_64_BIT_IMM(&emitted_block, emit_counter, (u64)&cpu->special_purpose_registers.lr,
                            HOST_R17) // save pointer to LR Register in HOST_R17
            LOAD_64_BIT_IMM(&emitted_block, emit_counter, (u64)pc_buffer[pc_buffer_counter],
                            HOST_R18) // save current pc in r18

            emit_ldr(&emitted_block, HOST_R18, HOST_R18, true, STR_POST_INDEX,
                     4); // save the pc in r18 and adds 4 after

            emit_str(&emitted_block, HOST_R18, HOST_R17, false, STR_POST_INDEX, 0x0);
        }
        *pc_after_instruction = LI;

        break;
    }
    case OPC_LFD: {
        // jmp

        // else block

        // finaly block
        break;
    }
    }

    _print_instruction(&emitted_block);
}

TranslationBlock *tb_translate(CPU *cpu, CpuMode cpu_mode)
{
    u32 pc = cpu->state.pc;

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
