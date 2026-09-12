#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/asm_emit.h"
#include "cpu/translation/emit.h"
#include "translation.h"
#include "translation_core_defines.h"
#include <_abort.h>
#include <_string.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define CORRECT_ENDIAN(x) (_endian32(x, is_little_endian))

#define TERMINATING_TYPE_CONDITINIAL_BRANCH 1
#define TERMINATING_TYPE_MSR_CHANGE 2

#define ASM_RET 0xD65F03C0
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
static inline i32 _sign_extend(u32 v, u8 bits)
{
    const u32 m = 1u << (bits - 1);
    return (i32)((v ^ m) - m);
}
static inline void _print_code_block(u32 *block, size_t max_len)
{

    char hex[0x4000] = {0};
    char spaced[16 * 512] = {0};

    u32 instructions_pushed = 0;
    for (int i = 0; i < max_len && i < 0x4000 / 4; i++) {
        if (block[i] == 0) {
            break;
        }
        u32 v = block[i];
        snprintf(hex + instructions_pushed * 8, sizeof(hex) - i * 8, "%08x", _endian32(v, false));
        instructions_pushed++;
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

struct block {
    const void *start;
    const void *end;
};
static u32 *write_buffer_impl(u32 *buffer, const struct block *blocks, size_t n)
{
    u8 *out = (u8 *)buffer;
    for (size_t i = 0; i < n; i++) {
        if (!blocks[i].start || !blocks[i].end)
            continue;
        size_t len = (const u8 *)blocks[i].end - (const u8 *)blocks[i].start;
        memcpy(out, blocks[i].start, len);
        out += len;
    }
    return (u32 *)out;
}

#define write_to_buffer(buffer, ...)                                                               \
    write_buffer_impl((buffer), (const struct block[]){__VA_ARGS__},                               \
                      sizeof((const struct block[]){__VA_ARGS__}) / sizeof(struct block))

static u32 *_translate_instruction(u32 insn, CPU *cpu, u32 *pc_after_instruction, u32 *pc_buffer,
                                   u32 pc_buffer_counter, u32 *code_buffer, u8 *termination_type)
{

    u32 host_instruction_counter = 0;
    const u8 op = _get_op_from_instruction(insn);
    printf("instuction : 0x%08x\n", insn);
    printf("op : %d\n", op);

    // NOTE: the after_instruction_pc must be set in every case
    switch (op) {
    case OPC_ADDI: {

        const u32 rD_field = _get_field(insn, 6, 10);
        const u32 rA_field = _get_field(insn, 11, 15);
        const u32 *regD = &cpu->registers.gpio[rD_field];
        const i16 imm = _get_field(insn, 16, 31);
        u32 *curr = code_buffer;

        if (rA_field == 0) {

            curr = emit_load_u32(curr, 0, (u32)rD_field * 4);
            curr = emit_load_u32(curr, 1, (i32)imm);
            curr = emit_store_u32_indexed(curr, 1, GUEST_REGISTER_POINTER, 0, A64_EXT_LSL, 0);
        } else {
            const u32 *regA = &cpu->registers.gpio[rA_field];
            const u32 *blk, *blk_end;
            curr = emit_load_u32(curr, 0, (u64)rA_field);
            curr = emit_load_u32(curr, 1, (u64)rD_field);
            curr = emit_load_u32(curr, 2, (i32)imm);

            emit_addi(&blk, &blk_end);
            curr = write_to_buffer(curr, {blk, blk_end});
        }
        *pc_after_instruction += 4;
        return curr;
    }
    case OPC_ORI: {

        printf("[0x%08x] : ori r%d, r%d, imm(#%d / 0x%08x)\n", pc_buffer[pc_buffer_counter],
               _get_field(insn, 6, 10), _get_field(insn, 11, 15), _get_field(insn, 16, 31),
               _get_field(insn, 16, 31));

        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u16 imm = _get_field(insn, 16, 31);

        curr_instruction = emit_load_u64(curr_instruction, 0, (u64)regS);
        curr_instruction = emit_load_u64(curr_instruction, 1, (u64)regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)imm);

        const u32 *main_block;
        const u32 *main_block_end;
        emit_ori(&main_block, &main_block_end);

        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
        *pc_after_instruction += 4;

        return curr_instruction;
    }
    case OPC_BX: {
        const u8 AA = _get_bit(insn, 30);
        const u8 LK = _get_bit(insn, 31);
        const i32 LI = _sign_extend(_get_field(insn, 6, 29), 24);
        const i32 offset = LI * 4;

        printf("[0x%08x] : b%s%s  %d\n", pc_buffer[pc_buffer_counter], LK ? "l" : "", AA ? "a" : "",
               offset);

        if (LK) {
            cpu->special_purpose_registers.lr = pc_buffer[pc_buffer_counter] + 4;
        }

        if (AA) {
            *pc_after_instruction = (u32)offset; // absolut, sign-extended
        } else {
            *pc_after_instruction =
                pc_buffer[pc_buffer_counter] + (u32)offset; // relativ, wrapt korrekt in u32
        }

        return code_buffer;
    }
    case OPC_ADDIS: {

        printf("[0x%08x] : addis r%d, r%d, %d\n", pc_buffer[pc_buffer_counter],
               _get_field(insn, 6, 10), _get_field(insn, 11, 15), _get_field(insn, 16, 31));

        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 imm = _get_field(insn, 16, 31);

        if (_get_field(insn, 11, 15) == 0) {

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, (i32)(imm << 16));
            curr_instruction = emit_store_u32_indexed(curr_instruction, 1, GUEST_REGISTER_POINTER,
                                                      0, A64_EXT_UXTW, 2);
        } else {

            const u32 *main_block, *main_block_end;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)regA);
            curr_instruction = emit_load_u32(curr_instruction, 1, (u64)regD);
            curr_instruction = emit_load_u32(curr_instruction, 2, (i32)(imm << 16));

            emit_addi(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
        }
        *pc_after_instruction += 4;

        return curr_instruction;
    }
    case OPC_BCLR: {
        if (_get_field(insn, 21, 30) == OPC_BCLR_EXT) {
            if (insn == 0x4e800020) {
                // uncodininial branch (ret)

                printf("[0x%08x] : bclr  (RET)\n", pc_buffer[pc_buffer_counter]);

                printf("pc after : [0x%08x]   (RET)\n", cpu->special_purpose_registers.lr);
                *pc_after_instruction = cpu->special_purpose_registers.lr;

                return code_buffer;
                break;

            } else {
                assert(!"conditinial branch not implemented");
            }
        }
    }
    case OPC_MFMSR | OPC_MTMSR: {
        if (_get_field(insn, 21, 30) == OPC_MFMSR_EXT) {
            printf("[0x%08x] : mfmsr r%d, \n", pc_buffer[pc_buffer_counter],
                   _get_field(insn, 6, 10));

            const u32 regD = _get_field(insn, 6, 10);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u32)regD);
            curr_instruction = emit_load_u64(curr_instruction, 1, (u64)&cpu->state.msr);
            curr_instruction = emit_store_u32_indexed(curr_instruction, 1, GUEST_REGISTER_POINTER,
                                                      0, A64_EXT_UXTW, 2);

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_MTMSR_EXT) {
            printf("[0x%08x] : mtmsr r%d, \n", pc_buffer[pc_buffer_counter],
                   _get_field(insn, 6, 10));

            const u32 regD = _get_field(insn, 6, 10);

            u32 *curr_instruction = code_buffer;

            printf("regd : 0x%016x, msr : 0x%016x\n", (u64)regD, (u64)&cpu->state.msr);
            curr_instruction = emit_load_u64(curr_instruction, 0, (u64)&cpu->state.msr);
            curr_instruction = emit_load_u32(curr_instruction, 1, (u64)regD);

            const u32 *main_block, *main_block_end;
            emit_mtmsr(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;
            *termination_type = TERMINATING_TYPE_MSR_CHANGE;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_MFSPR_EXT) {
            printf("[0x%08x] : mfspr r%d, \n", pc_buffer[pc_buffer_counter],
                   _get_field(insn, 6, 10));

            const u8 regD = _get_field(insn, 6, 10);
            const u8 spr = _get_field(insn, 11, 20);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)regD);
            curr_instruction =
                emit_load_u64(curr_instruction, 1, (u64)&cpu->special_purpose_registers);

            curr_instruction = emit_load_u32(curr_instruction, 2, (u64)spr);

            const u32 *main_block, *main_block_end;
            emit_mtmsr(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            *termination_type = TERMINATING_TYPE_MSR_CHANGE;

            return curr_instruction;
        }
    }

    default:

        assert(!"not implemented guest instruction");
    }
}

static u32 *_push_pc_change_to_tb(u32 pc_to_push, u32 *code_buffer, CPU *cpu)
{
    u32 *curr_instruction = code_buffer;
    const u32 *pc_register_ptr = &cpu->state.pc;

    curr_instruction = emit_load_u64(curr_instruction, 0, (u64)pc_register_ptr);
    curr_instruction = emit_load_u32(curr_instruction, 1, (i32)pc_to_push);
    curr_instruction = emit_store_u32(curr_instruction, 1, 0, 0);

    return curr_instruction;
}

TranslationBlock tb_translate(CPU *cpu, CpuMode cpu_mode)
{

    void *host_code_buffer =
        mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (host_code_buffer == MAP_FAILED) {
        perror("mmap");
        abort();
    }

    memset(host_code_buffer, 0, 0x1000);
    u32 mmap_instructions_counter = 0;

    u32 pc = cpu->state.pc;

    bool is_little_endian = BIT_CHECK(cpu_mode.val, 31);

    u32 pc_during_instruction = pc;
    u32 pc_after_instruction = pc;

    u32 pc_buffer[MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK];
    u16 pc_buffer_counter = 0;
    u16 host_instructions_pushed_counter = 0;

    u32 *current_host_code_block_ptr =
        emit_load_u64(host_code_buffer, GUEST_REGISTER_POINTER, (u64)cpu->registers.gpio);

    u8 termination_type = 0;
    do {

        pc_during_instruction = pc_after_instruction;
        printf("current pc : 0x%016x\n", pc_during_instruction);
        pc_buffer[pc_buffer_counter] = pc_during_instruction;

        u32 *host_code_ptr_before_translation = current_host_code_block_ptr;
        u32 *current_instruction = cpu->bus->read(cpu->bus, pc_during_instruction);
        current_host_code_block_ptr = _translate_instruction(
            CORRECT_ENDIAN(*current_instruction), cpu, &pc_after_instruction, pc_buffer,
            pc_buffer_counter, current_host_code_block_ptr, &termination_type);
        _print_code_block(
            host_code_ptr_before_translation,
            ((u64)current_host_code_block_ptr - (u64)host_code_ptr_before_translation) / 4);
        pc_buffer_counter += 1;
        host_instructions_pushed_counter +=
            (u64)current_host_code_block_ptr - (u64)host_code_ptr_before_translation;

    } while (host_instructions_pushed_counter <
                 MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK - 128 &&
             termination_type == 0);

    if (termination_type == TERMINATING_TYPE_MSR_CHANGE) {
        // pc change has to be pushed to tb

        current_host_code_block_ptr =
            _push_pc_change_to_tb(pc_after_instruction, current_host_code_block_ptr, cpu);
    }

    *current_host_code_block_ptr = 0xD65F03C0;
    _print_code_block(host_code_buffer, host_instructions_pushed_counter);

    TranslationBlock tb = {
        .core = {.code = host_code_buffer, .size = host_instructions_pushed_counter},
        .pc_at_start = cpu->state.pc,
        .msr_at_start = cpu->state.msr};
    return tb;
}
