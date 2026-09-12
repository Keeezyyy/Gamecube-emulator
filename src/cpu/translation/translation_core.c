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
static inline void _print_code_block(u32 *block, size_t max_len)
{

    char hex[512] = {0};
    char spaced[16 * 512] = {0};

    u32 instructions_pushed = 0;
    for (int i = 0; i < max_len; i++) {
        if (block[i] == 0) {
            break;
        }
        u32 v = block[i];
        printf("v : 0x%08x\n", v);
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

struct block {
    const void *start;
    const void *end;
};
void write_buffer_impl(u32 *buffer, const struct block *blocks, size_t n)
{
    u32 counter = 0;
    for (size_t i = 0; i < n; i++) {
        struct block b = blocks[i];

        u32 len = b.end - b.start;

        printf("start : 0x%016x , end : 0x%016x\n", b.start, b.end);
        printf("len : %d\n", len);
        memcpy(buffer + counter, b.start, len);
        counter += len;
    }
}

#define write_to_buffer(buffer, ...)                                                               \
    write_buffer_impl((buffer), (const struct block[]){__VA_ARGS__},                               \
                      sizeof((const struct block[]){__VA_ARGS__}) / sizeof(struct block))

static EmitedBlock _translate_instruction(u32 insn, CPU *cpu, u32 *pc_after_instruction,
                                          u32 *pc_buffer, u32 pc_buffer_counter)
{
    EmitedBlock emitted_block = {0};

    u32 host_instruction_counter = 0;
    const u8 op = _get_op_from_instruction(insn);
    printf("instuction : 0x%08x\n", insn);
    printf("op : %d\n", op);

    u32 code_buffer[128] = {0};

    // NOTE: the after_instruction_pc must be set in every case
    switch (op) {
    case OPC_ADDI: {

        const u32 *regD = &cpu->registers.gpio[_get_field(insn, 6, 10)];
        const u32 *regA = &cpu->registers.gpio[_get_field(insn, 11, 15)];
        const i16 imm = _get_field(insn, 16, 31);

        printf("[0x%08x] : addi r%d, r%d, %d\n", pc_buffer[pc_buffer_counter],
               _get_field(insn, 6, 10), _get_field(insn, 11, 15), _get_field(insn, 16, 31));

        const u32 *main_block;
        const u32 *main_block_end;
        emit_addi(&main_block, &main_block_end);

        u32 *curr_instruction = code_buffer;

        curr_instruction = emit_load_u64(curr_instruction, 0, (u64)regA);
        curr_instruction = emit_load_u64(curr_instruction, 1, (u64)regD);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)imm);
        write_to_buffer(curr_instruction, {main_block, main_block_end});
        *pc_after_instruction += 4;

        break;
    }
    case OPC_ORI: {

        printf("[0x%08x] : ori r%d, r%d, imm(%d)\n", pc_buffer[pc_buffer_counter],
               _get_field(insn, 6, 10), _get_field(insn, 11, 15), _get_field(insn, 16, 31));

        const u32 *regS = &cpu->registers.gpio[_get_field(insn, 6, 10)];
        const u32 *regA = &cpu->registers.gpio[_get_field(insn, 11, 15)];
        const u16 imm = _get_field(insn, 16, 31);

        const u32 *main_block;
        const u32 *main_block_end;
        emit_ori(&main_block, &main_block_end);

        u32 *curr_instruction = code_buffer;

        curr_instruction = emit_load_u64(curr_instruction, 0, (u64)regS);
        curr_instruction = emit_load_u64(curr_instruction, 1, (u64)regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)imm);
        write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;
        break;
    }
    case OPC_BX: {

        printf("[0x%08x] : bx  %d\n", pc_buffer[pc_buffer_counter], _get_field(insn, 6, 29));

        const u8 AA = _get_field(insn, 30, 30);
        const u8 LK = _get_field(insn, 31, 31);
        const i32 LI = _get_field(insn, 6, 29);

        const u32 *main_block = 0;
        const u32 *main_block_end = 0;

        u32 *curr_instruction = code_buffer;

        if (AA == 1) {

            *pc_after_instruction = LI << 2;
        } else {
            *pc_after_instruction = pc_buffer[pc_buffer_counter] + (LI << 2);
        }

        if (LK == 1) {
            cpu->special_purpose_registers.lr = pc_buffer[pc_buffer_counter] + 4;
        }

        write_to_buffer(curr_instruction, {main_block, main_block_end});

        break;
    }
    case OPC_ADDIS: {

        printf("[0x%08x] : addis r%d, r%d, %d\n", pc_buffer[pc_buffer_counter],
               _get_field(insn, 6, 10), _get_field(insn, 11, 15), _get_field(insn, 16, 31));

        u32 *curr_instruction = code_buffer;
        const u32 *regD = &cpu->registers.gpio[_get_field(insn, 6, 10)];
        const u32 *regA = &cpu->registers.gpio[_get_field(insn, 11, 15)];
        const i16 imm = _get_field(insn, 16, 31);

        if (_get_field(insn, 11, 15) == 0) {

            curr_instruction = emit_load_u64(curr_instruction, 0, (u64)regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, (i32)(imm << 16));
            emit_store_u32(curr_instruction, 1, 0, 0);
        } else {

            const u32 *main_block;
            const u32 *main_block_end;
            emit_addi(&main_block, &main_block_end);

            curr_instruction = emit_load_u64(curr_instruction, 0, (u64)regA);
            curr_instruction = emit_load_u64(curr_instruction, 1, (u64)regD);
            curr_instruction = emit_load_u32(curr_instruction, 2, (i32)(imm << 16));
            write_to_buffer(curr_instruction, {main_block, main_block_end});
        }
        *pc_after_instruction += 4;

        break;
    }
    case OPC_BCLR: {
        if (_get_field(insn, 21, 30) == OPC_BCLR_EXT) {
            if (insn == 0x4e800020) {
                // uncodininial branch (ret)

                printf("[0x%08x] : bclr  (RET)\n", pc_buffer[pc_buffer_counter]);

                printf("pc after : [0x%08x]   (RET)\n", cpu->special_purpose_registers.lr);
                *pc_after_instruction = cpu->special_purpose_registers.lr;
                break;

            } else {
                assert(!"conditinial branch not implemented");
            }
        }
    }
    case OPC_MFMSR: {
        printf("[0x%08x] : mfmsr r%d, \n", pc_buffer[pc_buffer_counter], _get_field(insn, 6, 10));

        const u32 *regD = &cpu->registers.gpio[_get_field(insn, 6, 10)];

        u32 *curr_instruction = code_buffer;

        printf("regd : 0x%016x, msr : 0x%016x\n", (u64)regD, (u64)&cpu->state.msr);
        curr_instruction = emit_load_u64(curr_instruction, 0, (u64)regD);
        curr_instruction = emit_load_u64(curr_instruction, 1, (u64)&cpu->state.msr);
        curr_instruction = emit_store_u32(curr_instruction, 0, 1, 0);

        *pc_after_instruction += 4;
        break;
    }
    default:

        assert(!"not implemented guest instruction");
    }
    _print_code_block(code_buffer, 128);
    return emitted_block;
}
static inline u32 _mask_from_range(u8 bit_start, u8 bit_end)
{
    u8 width = (u8)(bit_end - bit_start + 1u);
    if (width >= 32u)
        return 0xFFFFFFFFu;
    return ((1u << width) - 1u) << bit_start;
}

static void _write_patchup_to_instruction(void *adr_of_pointed_to_instruction, EmitPatch p)
{
    printf("EmitPatch { type=%d, instruction_ptr=%p, bit_mask=0x%08x, emitted_block_id=0x%llx, "
           "bit_start=%u, bit_end=%u, bit_shift=%d }\n",
           p.type, (void *)p.instruction_ptr, p.bit_mask, (unsigned long long)p.emitted_block_id,
           p.bit_start, p.bit_end, p.bit_shift);

    assert(p.instruction_ptr != NULL);
    assert(p.bit_end >= p.bit_start && p.bit_end < 32);

    u64 target = (u64)(uintptr_t)adr_of_pointed_to_instruction;
    u64 value;

    switch (p.type) {
    case PATCHING_TYPE_ADDRESS_TO_EMITTED_BLOCK:
        value = target;
        break;
    case PATCHING_TYPE_OFFSET_TO_EMITTED_BLOCK:
        value = target - (u64)(uintptr_t)p.instruction_ptr;
        break;
    default:
        value = target;
        break;
    }

    if (p.bit_shift > 0)
        value >>= (u32)p.bit_shift;
    else if (p.bit_shift < 0)
        value <<= (u32)(-(i32)p.bit_shift);

    u32 field = _mask_from_range(p.bit_start, p.bit_end);
    if (p.bit_mask)
        field &= p.bit_mask;

    u32 placed = ((u32)value << p.bit_start) & field;

    assert((((u32)value << p.bit_start) & ~field) == 0 && "patch value does not fit into field");

    *p.instruction_ptr = (*p.instruction_ptr & ~field) | placed;
}
static void _patch_up_Emitted_block(EmitedBlock *eb)
{
    for (int i = 0; i < eb->num_of_host_instrucion_blocks; i++) {
        HostInstructionsBlock *b = &eb->host_instruction_buffer[i];

        // memccpy the instructions

        if (!b->has_patch)
            continue;

        printf("case instruction : 0x%08x\n", *b->patch.instruction_ptr);
        u64 id = b->patch.emitted_block_id;
        bool found = false;
        for (int j = 0; j < eb->num_of_host_instrucion_blocks; j++) {
            if (eb->host_instruction_buffer[j].id != id)
                continue;
            assert(!found && "duplicate emitted_block_id");
            assert(eb->host_instruction_buffer[j].ptr_to_final_location != NULL);
            _write_patchup_to_instruction(eb->host_instruction_buffer[j].ptr_to_final_location,
                                          b->patch);
            found = true;
        }
        assert(found && "patch target id not emitted");
    }
}
static void _write_emitted_block_into_mmap(void *mmap, u32 *mmap_instructions_counter,
                                           EmitedBlock *eb)
{
    for (int i = 0; i < eb->num_of_host_instrucion_blocks; i++) {
        HostInstructionsBlock *b = &eb->host_instruction_buffer[i];

        // memccpy the instructions

        memcpy(((u32 *)mmap) + *mmap_instructions_counter, b->host_code_buffer,
               (b->num_of_instructions) * 4);

        b->ptr_to_final_location = ((u32 *)mmap) + *mmap_instructions_counter;
        if (b->has_patch) {
            printf("pointer to mmap instruciton : 0x%08x\n",
                   ((u32 *)mmap) + *mmap_instructions_counter);
            b->patch.instruction_ptr = ((u32 *)mmap) + *mmap_instructions_counter;
        }
        *mmap_instructions_counter += b->num_of_instructions;
    }
    _patch_up_Emitted_block(eb);
}

static void _print_final_tb(u32 *mmap, u32 size)
{
    char hex[512] = {0};
    char spaced[16 * 512] = {0};

    u32 instructions_pushed = 0;
    for (int i = 0; i < size; i++) {
        u32 v = mmap[i];
        printf("v : 0x%08x\n", v);
        snprintf(hex + instructions_pushed * 8, sizeof(hex) - i * 8, "%08x", _endian32(v, false));
        instructions_pushed++;
    }

    size_t n = 0;
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2)
        n += snprintf(spaced + n, sizeof(spaced) - n, "0x%c%c ", hex[i], hex[i + 1]);

    printf("-----------------------------------------------------------------------------------"
           "----"
           "-----------------------------------------------------------------------------------"
           "----"
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

    printf("-----------------------------------------------------------------------------------"
           "----"
           "-----------------------------------------------------------------------------------"
           "----"
           "------------------------------------\n");
}

TranslationBlock *tb_translate(CPU *cpu, CpuMode cpu_mode)
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
    do {

        pc_during_instruction = pc_after_instruction;
        printf("current pc : 0x%016x\n", pc_during_instruction);
        pc_buffer[pc_buffer_counter] = pc_during_instruction;
        u32 *current_instruction = cpu->bus->read(cpu->bus, pc_during_instruction);
        EmitedBlock out =
            _translate_instruction(CORRECT_ENDIAN(*current_instruction), cpu, &pc_after_instruction,
                                   pc_buffer, pc_buffer_counter);

        pc_buffer_counter++;

    } while (pc_buffer_counter < MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK);
}
