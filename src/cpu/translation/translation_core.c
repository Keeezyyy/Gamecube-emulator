#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/emit/asm_emit.h"
#include "cpu/translation/emit/emit.h"
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

#define CORRECT_ENDIAN(x) (_endian32(x, false))

#define TERMINATING_TYPE_CONDITINIAL_BRANCH 1
#define TERMINATING_TYPE_MSR_CHANGE 2
#define TERMINATING_TYPE_SPR_CHANGE 3
#define TERMINATING_TYPE_RET 4

#define ASM_RET 0xD65F03C0

// runtime debug switch, set by tb_translate(..., print_debug)
static bool g_print_debug = false;
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

    if (g_print_debug)
        printf("-----------------------------------------------------------------------------------"
               "----"
               "-----------------------------------------------------------------------------------"
               "----"
               "------------------------------------\n");
    if (g_print_debug)
        printf("disasm %s\n", hex);
    fflush(stdout);

    FILE *p = popen("xcrun llvm-mc --disassemble -triple=arm64", "w");
    if (!p) {
        perror("popen");
        return;
    }
    fprintf(p, "%s\n", spaced);
    pclose(p);

    if (g_print_debug)
        printf("-----------------------------------------------------------------------------------"
               "----"
               "-----------------------------------------------------------------------------------"
               "----"
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
        assert(blocks[i].start <= blocks[i].end);
        size_t len = (const u8 *)blocks[i].end - (const u8 *)blocks[i].start;
        memcpy(out, blocks[i].start, len);
        out += len;
    }
    return (u32 *)out;
}

static bool is_pc_in_current_tb(u32 *pc_buffer, u32 current_pc_index, u32 dest_pc, u32 *pc_index)
{
    for (int i = 0; i < current_pc_index; i++) {
        if (pc_buffer[i] == dest_pc) {
            *pc_index = i;
            return true;
        }
    }
    return false;
}
void special()
{
    if (g_print_debug)
        printf("b");
}

static u32 *_translate_instruction(u32 insn, CPU *cpu, u32 *pc_after_instruction, u32 *pc_buffer,
                                   u32 **host_block_buffer, u32 pc_buffer_counter, u32 *code_buffer,
                                   u8 *termination_type, u8 *tb_type, FPRUsageBitmap *fpr_bitmap)
{

    u32 host_instruction_counter = 0;
    const u8 op = _get_op_from_instruction(insn);
    if (g_print_debug)
        printf("instuction : 0x%08x\n", insn);
    if (g_print_debug)
        printf("op : %d\n", op);

    // NOTE: the after_instruction_pc must be set in every case
    switch (op) {
    case OPC_LFS:
    case OPC_LFSU:
    case OPC_LFD:
    case OPC_LFDU:
    case OPC_FMR: {
        // FLOATING POINT OPERATION

        *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
        switch (op) {
        case OPC_LFS: {
            u32 *curr_instruction = code_buffer;
            const u32 fD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const i32 d = _sign_extend(_get_field(insn, 16, 31), 16);

            if (g_print_debug)
                printf("[0x%08x] : lfs f%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], fD, regA,
                       d);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, (u32)d);
            curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->fpu.fpr[fD]);

            const u32 *main_block, *main_block_end;
            emit_lfs(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_set_ps0_from_gpr(curr_instruction, fD, 0);

            if (cpu->fpu.get_pse_bit(cpu) != 0)
                curr_instruction = emit_set_ps1_from_gpr(curr_instruction, fD, 0);

            *pc_after_instruction += 4;

            return curr_instruction;
            break;
        }
        case OPC_LFSU: {
            u32 *curr_instruction = code_buffer;
            const u32 fD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const i32 d = _sign_extend(_get_field(insn, 16, 31), 16);

            if (g_print_debug)
                printf("[0x%08x] : lfsu f%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], fD, regA,
                       d);

            assert(regA != 0);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, (u32)d);
            curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->fpu.fpr[fD]);

            const u32 *main_block, *main_block_end;
            emit_lfsu(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_set_ps0_from_gpr(curr_instruction, fD, 0);

            if (cpu->fpu.get_pse_bit(cpu) != 0)
                curr_instruction = emit_set_ps1_from_gpr(curr_instruction, fD, 0);

            *pc_after_instruction += 4;

            return curr_instruction;
            break;
        }
        case OPC_LFD: {
            u32 *curr_instruction = code_buffer;
            const u32 fD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const i32 d = _sign_extend(_get_field(insn, 16, 31), 16);

            if (g_print_debug)
                printf("[0x%08x] : lfd f%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], fD, regA,
                       d);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, (u32)d);
            curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->fpu.fpr[fD]);

            const u32 *main_block, *main_block_end;
            emit_lfd(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            // lfd setzt nur ps0, ps1 bleibt stehen
            curr_instruction = emit_set_ps0_from_gpr(curr_instruction, fD, 0);

            *pc_after_instruction += 4;

            return curr_instruction;
            break;
        }
        case OPC_LFDU: {
            u32 *curr_instruction = code_buffer;
            const u32 fD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const i32 d = _sign_extend(_get_field(insn, 16, 31), 16);

            if (g_print_debug)
                printf("[0x%08x] : lfdu f%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], fD, regA,
                       d);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, (u32)d);
            curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->fpu.fpr[fD]);

            const u32 *main_block, *main_block_end;
            emit_lfdu(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_set_ps0_from_gpr(curr_instruction, fD, 0);

            *pc_after_instruction += 4;

            return curr_instruction;
            break;
        }
        case OPC_FMR: {
            if (_get_field(insn, 21, 30) == OPC_FMR_EXT) {

                const u32 d = _get_field(insn, 6, 10);
                const u32 b = _get_field(insn, 16, 20);
                const u32 rc = _get_field(insn, 31, 31);

                if (g_print_debug)
                    printf("[0x%08x] : fmr%s f%d, f%d\n", pc_buffer[pc_buffer_counter],
                           rc ? "." : "", d, b);

                u32 *curr = code_buffer;
                curr = emit_set_ps0_from_float(curr, d, b);

                if (rc == 1) {
                    curr = emit_load_u64(curr, 15, (u64)&cpu->state.cr);
                    curr = emit_load_u64(curr, 16, (u64)&cpu->state.fpscr);

                    u32 *blk, *blk_end;
                    copy_fpscr_to_cr1(&blk, &blk_end);
                    curr = write_to_buffer(curr, {blk, blk_end});
                }

                *pc_after_instruction += 4;
                return curr;

            } else if (_get_field(insn, 21, 30) == OPC_FNEG_EXT) {

                const u32 d = _get_field(insn, 6, 10);
                const u32 b = _get_field(insn, 16, 20);
                const u32 rc = _get_field(insn, 31, 31);

                if (g_print_debug)
                    printf("[0x%08x] : fneg%s f%d, f%d\n", pc_buffer[pc_buffer_counter],
                           rc ? "." : "", d, b);

                u32 *curr = code_buffer;
                curr = emit_fmov_into_gpr_64(curr, 9, b);

                u32 *blk, *blk_end;
                emit_fneg(&blk, &blk_end);
                curr = write_to_buffer(curr, {blk, blk_end});

                curr = emit_set_ps0_from_gpr(curr, d, 9);

                if (rc == 1) {
                    curr = emit_load_u64(curr, 15, (u64)&cpu->state.cr);
                    curr = emit_load_u64(curr, 16, (u64)&cpu->state.fpscr);

                    copy_fpscr_to_cr1(&blk, &blk_end);
                    curr = write_to_buffer(curr, {blk, blk_end});
                }

                *pc_after_instruction += 4;
                return curr;

            } else if (_get_field(insn, 21, 30) == OPC_FABS_EXT) {

                const u32 d = _get_field(insn, 6, 10);
                const u32 b = _get_field(insn, 16, 20);
                const u32 rc = _get_field(insn, 31, 31);

                if (g_print_debug)
                    printf("[0x%08x] : fabs%s f%d, f%d\n", pc_buffer[pc_buffer_counter],
                           rc ? "." : "", d, b);

                u32 *curr = code_buffer;
                curr = emit_fmov_into_gpr_64(curr, 9, b);

                u32 *blk, *blk_end;
                emit_fabs(&blk, &blk_end);
                curr = write_to_buffer(curr, {blk, blk_end});

                curr = emit_set_ps0_from_gpr(curr, d, 9);

                if (rc == 1) {
                    curr = emit_load_u64(curr, 15, (u64)&cpu->state.cr);
                    curr = emit_load_u64(curr, 16, (u64)&cpu->state.fpscr);

                    copy_fpscr_to_cr1(&blk, &blk_end);
                    curr = write_to_buffer(curr, {blk, blk_end});
                }

                *pc_after_instruction += 4;
                return curr;

            } else if (_get_field(insn, 21, 30) == OPC_FNABS_EXT) {

                const u32 d = _get_field(insn, 6, 10);
                const u32 b = _get_field(insn, 16, 20);
                const u32 rc = _get_field(insn, 31, 31);

                if (g_print_debug)
                    printf("[0x%08x] : fnabs%s f%d, f%d\n", pc_buffer[pc_buffer_counter],
                           rc ? "." : "", d, b);

                u32 *curr = code_buffer;
                curr = emit_fmov_into_gpr_64(curr, 9, b);

                u32 *blk, *blk_end;
                emit_fnabs(&blk, &blk_end);
                curr = write_to_buffer(curr, {blk, blk_end});

                curr = emit_set_ps0_from_gpr(curr, d, 9);

                if (rc == 1) {
                    curr = emit_load_u64(curr, 15, (u64)&cpu->state.cr);
                    curr = emit_load_u64(curr, 16, (u64)&cpu->state.fpscr);

                    copy_fpscr_to_cr1(&blk, &blk_end);
                    curr = write_to_buffer(curr, {blk, blk_end});
                }

                *pc_after_instruction += 4;
                return curr;

            } else {
                assert(!"not implemented floating point");
            }
        }
        default:
            assert(!"not implemented floating point");
        }

        assert(!"error in fpo");
    }
    case OPC_ADDI: {

        if (g_print_debug)
            printf("[0x%08x] : addi r%d, r%d, imm(#%d / 0x%08x)\n", pc_buffer[pc_buffer_counter],

                   _get_field(insn, 6, 10), _get_field(insn, 11, 15), _get_field(insn, 16, 31),
                   _get_field(insn, 16, 31));

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
    case OPC_ADDIC: {

        const u32 rD_field = _get_field(insn, 6, 10);
        const u32 rA_field = _get_field(insn, 11, 15);

        const i32 imm = _sign_extend(_get_field(insn, 16, 31), 16);

        if (g_print_debug)
            printf("[0x%08x] : addic r%d, r%d, imm(#%d / 0x%08x)\n", pc_buffer[pc_buffer_counter],
                   rD_field, rA_field, imm);
        u32 *curr = code_buffer;

        const u32 *regA = &cpu->registers.gpio[rA_field];
        u32 *blk, *blk_end;
        curr = emit_load_u32(curr, 0, (u64)rA_field);
        curr = emit_load_u32(curr, 1, (u64)rD_field);
        curr = emit_load_u32(curr, 2, (i32)imm);

        emit_addic(&blk, &blk_end);
        curr = write_to_buffer(curr, {blk, blk_end});

        curr = emit_load_u64(curr, 16, (u64)&cpu->state.xer);

        set_xer_ca_from_w15(&blk, &blk_end);
        curr = write_to_buffer(curr, {blk, blk_end});
        *pc_after_instruction += 4;
        return curr;
    }
    case OPC_ADDIC_CR0: {

        const u32 rD_field = _get_field(insn, 6, 10);
        const u32 rA_field = _get_field(insn, 11, 15);
        const i32 imm = _sign_extend(_get_field(insn, 16, 31), 16);

        if (g_print_debug)
            printf("[0x%08x] : addic. r%d, r%d, imm(#%d / 0x%08x)\n", pc_buffer[pc_buffer_counter],
                   rD_field, rA_field, imm);
        u32 *curr = code_buffer;

        const u32 *regA = &cpu->registers.gpio[rA_field];
        u32 *blk, *blk_end;
        curr = emit_load_u32(curr, 0, (u64)rA_field);
        curr = emit_load_u32(curr, 1, (u64)rD_field);
        curr = emit_load_u32(curr, 2, (i32)imm);

        emit_addic(&blk, &blk_end);
        curr = write_to_buffer(curr, {blk, blk_end});

        curr = emit_load_u64(curr, 16, (u64)&cpu->state.xer);
        set_xer_ca_from_w15(&blk, &blk_end);
        curr = write_to_buffer(curr, {blk, blk_end});

        curr = emit_load_u64(curr, 5, (u64)&cpu->state.cr);
        set_cr0_from_w15(&blk, &blk_end);
        curr = write_to_buffer(curr, {blk, blk_end});
        *pc_after_instruction += 4;
        return curr;
    }
    case OPC_ORI: {

        if (g_print_debug)
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

        if (g_print_debug)
            printf("[0x%08x] : b%s%s  %d\n", pc_buffer[pc_buffer_counter], LK ? "l" : "",
                   AA ? "a" : "", offset);

        u32 *curr_instruction = code_buffer;
        if (LK) {
            // cpu->special_purpose_registers.lr = pc_buffer[pc_buffer_counter] + 4;

            curr_instruction = emit_load_u32(curr_instruction, 0, pc_buffer[pc_buffer_counter] + 4);
            curr_instruction =
                emit_load_u64(curr_instruction, 1, (u64)&cpu->special_purpose_registers.lr);

            const u32 *main_block;
            const u32 *main_block_end;
            emit_bx(&main_block, &main_block_end);

            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
        }

        if (AA) {
            *pc_after_instruction = (u32)offset; // absolut, sign-extended
        } else {
            *pc_after_instruction =
                pc_buffer[pc_buffer_counter] + (u32)offset; // relativ, wrapt korrekt in u32
        }

        return curr_instruction;
    }
    case OPC_BCX: {

        const u8 b0 = _get_field(insn, 6, 10);
        const u8 bi = _get_field(insn, 11, 15);
        const i32 bd = _sign_extend(_get_field(insn, 16, 29) << 2, 14 + 2);
        const u8 AA = _get_bit(insn, 30);
        const u8 LK = _get_bit(insn, 31);

        u32 nia = AA == 1 ? bd : bd + pc_buffer[pc_buffer_counter];
        u32 pc_index = 0;
        if (is_pc_in_current_tb(pc_buffer, pc_buffer_counter, nia, &pc_index) == true) {
            // pc deistination is in current tb
            //  optimize to jump inside the tb
            if (g_print_debug)
                printf("[0x%08x] : bcx   %d\n", pc_buffer[pc_buffer_counter], bi);

            u32 *curr_instruction = code_buffer;
            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)b0);
            curr_instruction = emit_load_u32(curr_instruction, 1, (u64)31 - bi);
            curr_instruction = emit_load_u32(curr_instruction, 2, (u64)AA);
            curr_instruction = emit_load_u32(curr_instruction, 3, (u64)LK);
            curr_instruction = emit_load_u32(curr_instruction, 4, bd);
            curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);

            curr_instruction = emit_load_u32(curr_instruction, 12, pc_buffer[pc_buffer_counter]);
            curr_instruction = emit_load_u64(curr_instruction, 13, (u64)&cpu->state.pc);
            curr_instruction =
                emit_load_u64(curr_instruction, 14, (u64)&cpu->special_purpose_registers.lr);
            curr_instruction =
                emit_load_u64(curr_instruction, 15, (u64)&cpu->special_purpose_registers.ctr);
            curr_instruction =
                emit_load_u64(curr_instruction, 16, (u64)host_block_buffer[pc_index]);

            const u32 *main_block;
            const u32 *main_block_end;
            emit_bcx_jump_in_tb(&main_block, &main_block_end);

            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;
            return curr_instruction;
        } else {

            if (g_print_debug)
                printf("[0x%08x] : bcx   %d\n", pc_buffer[pc_buffer_counter], bi);

            u32 *curr_instruction = code_buffer;
            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)b0);
            curr_instruction = emit_load_u32(curr_instruction, 1, (u64)31 - bi);
            curr_instruction = emit_load_u32(curr_instruction, 2, (u64)AA);
            curr_instruction = emit_load_u32(curr_instruction, 3, (u64)LK);
            curr_instruction = emit_load_u32(curr_instruction, 4, bd);
            curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);

            curr_instruction = emit_load_u32(curr_instruction, 12, pc_buffer[pc_buffer_counter]);
            curr_instruction = emit_load_u64(curr_instruction, 13, (u64)&cpu->state.pc);
            curr_instruction =
                emit_load_u64(curr_instruction, 14, (u64)&cpu->special_purpose_registers.lr);
            curr_instruction =
                emit_load_u64(curr_instruction, 15, (u64)&cpu->special_purpose_registers.ctr);

            const u32 *main_block;
            const u32 *main_block_end;
            emit_bcx(&main_block, &main_block_end);

            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *termination_type = TERMINATING_TYPE_CONDITINIAL_BRANCH;

            *pc_after_instruction += 4;
            return curr_instruction;
        }
    }
    case OPC_ADDIS: {

        if (g_print_debug)
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

                if (g_print_debug)
                    printf("[0x%08x] : blr  (RET)\n", pc_buffer[pc_buffer_counter]);

                if (g_print_debug)
                    printf("pc after : [0x%08x]   (RET)\n", cpu->special_purpose_registers.lr);
                *pc_after_instruction = cpu->special_purpose_registers.lr;

                const u32 *main_block, *main_block_end;
                u32 *curr_instruction = code_buffer;
                curr_instruction = emit_load_u64(curr_instruction, 0, (u64)&cpu->state.pc);
                curr_instruction =
                    emit_load_u64(curr_instruction, 1, (u64)&cpu->special_purpose_registers.lr);

                emit_blr(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

                *termination_type = TERMINATING_TYPE_RET;

                return curr_instruction;
                break;
            } else {
                const u32 bo = _get_field(insn, 6, 10);
                const u32 bi = _get_field(insn, 11, 15);
                const u32 LK = _get_field(insn, 31, 31);
                if (g_print_debug)
                    printf("[0x%08x] : bclr %d, %d \n", pc_buffer[pc_buffer_counter], bo, bi);
                u32 *curr_instruction = code_buffer;

                curr_instruction = emit_load_u32(curr_instruction, 0, (u64)bo);
                curr_instruction = emit_load_u32(curr_instruction, 1, (u64)31 - bi);
                curr_instruction = emit_load_u32(curr_instruction, 2, (u64)31 - bi);
                curr_instruction = emit_load_u32(curr_instruction, 3, (u64)LK);

                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);

                curr_instruction =
                    emit_load_u32(curr_instruction, 12, pc_buffer[pc_buffer_counter]);
                curr_instruction = emit_load_u64(curr_instruction, 13, (u64)&cpu->state.pc);
                curr_instruction =
                    emit_load_u64(curr_instruction, 14, (u64)&cpu->special_purpose_registers.lr);
                curr_instruction =
                    emit_load_u64(curr_instruction, 15, (u64)&cpu->special_purpose_registers.ctr);

                const u32 *main_block;
                const u32 *main_block_end;
                emit_bclr(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

                *termination_type = TERMINATING_TYPE_CONDITINIAL_BRANCH;
                return curr_instruction;
            }
        } else if (_get_field(insn, 21, 30) == OPC_ISYNC_EXT) {
            if (g_print_debug)
                printf("[0x%08x] : isync \n", pc_buffer[pc_buffer_counter]);
            u32 *curr_instruction = code_buffer;
            *pc_after_instruction += 4;

            return curr_instruction;
        } else if (_get_field(insn, 21, 30) == OPC_CRXOR_EXT) {
            const u32 crbD = _get_field(insn, 6, 10);
            const u32 crbA = _get_field(insn, 11, 15);
            const u32 crbB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : crxor %d, %d, %d \n", pc_buffer[pc_buffer_counter], crbD, crbA,
                       crbB);

            u32 *curr_instruction = code_buffer;
            curr_instruction = emit_load_u32(curr_instruction, 0, 31 - crbD);
            curr_instruction = emit_load_u32(curr_instruction, 1, 31 - crbA);
            curr_instruction = emit_load_u32(curr_instruction, 2, 31 - crbB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->state.cr);
            const u32 *main_block, *main_block_end;
            emit_crxor(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;

        } else if (_get_field(insn, 21, 30) == OPC_RFI_EXT) {

            if (true)
                printf("[0x%08x] : rfi \n", pc_buffer[pc_buffer_counter]);

            u32 *curr_instruction = code_buffer;
            const u32 *main_block, *main_block_end;
            emit_rfi(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            *termination_type = TERMINATING_TYPE_RET;

            return curr_instruction;

        } else if (_get_field(insn, 21, 30) == OPC_CRAND_EXT) {
            const u32 crbD = _get_field(insn, 6, 10);
            const u32 crbA = _get_field(insn, 11, 15);
            const u32 crbB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : crand %d, %d, %d \n", pc_buffer[pc_buffer_counter], crbD, crbA,
                       crbB);

            u32 *curr_instruction = code_buffer;
            curr_instruction = emit_load_u32(curr_instruction, 0, 31 - crbD);
            curr_instruction = emit_load_u32(curr_instruction, 1, 31 - crbA);
            curr_instruction = emit_load_u32(curr_instruction, 2, 31 - crbB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->state.cr);
            const u32 *main_block, *main_block_end;
            emit_crand(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;

        } else if (_get_field(insn, 21, 30) == OPC_CRANDC_EXT) {
            const u32 crbD = _get_field(insn, 6, 10);
            const u32 crbA = _get_field(insn, 11, 15);
            const u32 crbB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : crandc %d, %d, %d \n", pc_buffer[pc_buffer_counter], crbD, crbA,
                       crbB);

            u32 *curr_instruction = code_buffer;
            curr_instruction = emit_load_u32(curr_instruction, 0, 31 - crbD);
            curr_instruction = emit_load_u32(curr_instruction, 1, 31 - crbA);
            curr_instruction = emit_load_u32(curr_instruction, 2, 31 - crbB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->state.cr);
            const u32 *main_block, *main_block_end;
            emit_crandc(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;

        } else if (_get_field(insn, 21, 30) == OPC_CREQV_EXT) {
            const u32 crbD = _get_field(insn, 6, 10);
            const u32 crbA = _get_field(insn, 11, 15);
            const u32 crbB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : creqv %d, %d, %d \n", pc_buffer[pc_buffer_counter], crbD, crbA,
                       crbB);

            u32 *curr_instruction = code_buffer;
            curr_instruction = emit_load_u32(curr_instruction, 0, 31 - crbD);
            curr_instruction = emit_load_u32(curr_instruction, 1, 31 - crbA);
            curr_instruction = emit_load_u32(curr_instruction, 2, 31 - crbB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->state.cr);
            const u32 *main_block, *main_block_end;
            emit_creqv(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;

        } else if (_get_field(insn, 21, 30) == OPC_CRNAND_EXT) {
            const u32 crbD = _get_field(insn, 6, 10);
            const u32 crbA = _get_field(insn, 11, 15);
            const u32 crbB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : crnand %d, %d, %d \n", pc_buffer[pc_buffer_counter], crbD, crbA,
                       crbB);

            u32 *curr_instruction = code_buffer;
            curr_instruction = emit_load_u32(curr_instruction, 0, 31 - crbD);
            curr_instruction = emit_load_u32(curr_instruction, 1, 31 - crbA);
            curr_instruction = emit_load_u32(curr_instruction, 2, 31 - crbB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->state.cr);
            const u32 *main_block, *main_block_end;
            emit_crnand(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;

        } else if (_get_field(insn, 21, 30) == OPC_CRNOR_EXT) {
            const u32 crbD = _get_field(insn, 6, 10);
            const u32 crbA = _get_field(insn, 11, 15);
            const u32 crbB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : crnor %d, %d, %d \n", pc_buffer[pc_buffer_counter], crbD, crbA,
                       crbB);

            u32 *curr_instruction = code_buffer;
            curr_instruction = emit_load_u32(curr_instruction, 0, 31 - crbD);
            curr_instruction = emit_load_u32(curr_instruction, 1, 31 - crbA);
            curr_instruction = emit_load_u32(curr_instruction, 2, 31 - crbB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->state.cr);
            const u32 *main_block, *main_block_end;
            emit_crnor(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;

        } else if (_get_field(insn, 21, 30) == OPC_CROR_EXT) {
            const u32 crbD = _get_field(insn, 6, 10);
            const u32 crbA = _get_field(insn, 11, 15);
            const u32 crbB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : cror %d, %d, %d \n", pc_buffer[pc_buffer_counter], crbD, crbA,
                       crbB);

            u32 *curr_instruction = code_buffer;
            curr_instruction = emit_load_u32(curr_instruction, 0, 31 - crbD);
            curr_instruction = emit_load_u32(curr_instruction, 1, 31 - crbA);
            curr_instruction = emit_load_u32(curr_instruction, 2, 31 - crbB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->state.cr);
            const u32 *main_block, *main_block_end;
            emit_cror(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;

        } else if (_get_field(insn, 21, 30) == OPC_CRORC_EXT) {
            const u32 crbD = _get_field(insn, 6, 10);
            const u32 crbA = _get_field(insn, 11, 15);
            const u32 crbB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : crorc %d, %d, %d \n", pc_buffer[pc_buffer_counter], crbD, crbA,
                       crbB);

            u32 *curr_instruction = code_buffer;
            curr_instruction = emit_load_u32(curr_instruction, 0, 31 - crbD);
            curr_instruction = emit_load_u32(curr_instruction, 1, 31 - crbA);
            curr_instruction = emit_load_u32(curr_instruction, 2, 31 - crbB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->state.cr);
            const u32 *main_block, *main_block_end;
            emit_crorc(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;

        } else if (_get_field(insn, 21, 30) == OPC_BCCTR_EXT) {
            const u32 bo = _get_field(insn, 6, 10);
            const u32 bi = _get_field(insn, 11, 15);
            const u32 LK = _get_field(insn, 31, 31);
            if (g_print_debug)
                printf("[0x%08x] : bcctr %d, %d \n", pc_buffer[pc_buffer_counter], bo, bi);
            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)bo);
            curr_instruction = emit_load_u32(curr_instruction, 1, (u64)31 - bi);
            curr_instruction = emit_load_u32(curr_instruction, 3, (u64)LK);

            curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);

            curr_instruction = emit_load_u32(curr_instruction, 12, pc_buffer[pc_buffer_counter]);
            curr_instruction = emit_load_u64(curr_instruction, 13, (u64)&cpu->state.pc);
            curr_instruction =
                emit_load_u64(curr_instruction, 14, (u64)&cpu->special_purpose_registers.lr);
            curr_instruction =
                emit_load_u64(curr_instruction, 15, (u64)&cpu->special_purpose_registers.ctr);

            const u32 *main_block;
            const u32 *main_block_end;
            emit_bcctr(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *termination_type = TERMINATING_TYPE_CONDITINIAL_BRANCH;
            return curr_instruction;
        } else {
            assert(!"not implemeted 2.\n");
        }
    }
    case OPC_MFMSR | OPC_MTMSR: {
        if (_get_field(insn, 21, 30) == OPC_MFMSR_EXT) {
            if (g_print_debug)
                printf("[0x%08x] : mfmsr r%d, \n", pc_buffer[pc_buffer_counter],
                       _get_field(insn, 6, 10));

            const u32 regD = _get_field(insn, 6, 10);

            u32 *curr_instruction = code_buffer;

            const u32 *main_block, *main_block_end;
            curr_instruction = emit_load_u32(curr_instruction, 0, (u32)regD);
            curr_instruction = emit_load_u64(curr_instruction, 1, (u64)&cpu->state.msr);
            emit_mfmsr(&main_block, &main_block_end);

            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            *pc_after_instruction += 4;

            return curr_instruction;
        }
        // TODO: check manual for deeper functions and if it changes machine context
        // TODO: check manual for deeper functions and if it changes machine context
        // TODO: check manual for deeper functions and if it changes machine context
        // TODO: check manual for deeper functions and if it changes machine context
        if (_get_field(insn, 21, 30) == OPC_MTMSR_EXT) {
            if (g_print_debug)
                printf("[0x%08x] : mtmsr r%d, \n", pc_buffer[pc_buffer_counter],
                       _get_field(insn, 6, 10));

            const u32 regD = _get_field(insn, 6, 10);

            u32 *curr_instruction = code_buffer;

            if (g_print_debug)
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
        if (_get_field(insn, 21, 30) == OPC_MFSPR_EXT || _get_field(insn, 21, 30) == OPC_MFTB_EXT) {
            const u32 lo = _get_field(insn, 11, 15);
            const u32 hi = _get_field(insn, 16, 20);
            const u32 spr = (hi << 5) | lo;

            if (_get_field(insn, 21, 30) == OPC_MFSPR_EXT) {
                if (g_print_debug)
                    printf("[0x%08x] : mfspr r%d, spr%d\n", pc_buffer[pc_buffer_counter],
                           _get_field(insn, 6, 10), spr);
            } else {
                if (g_print_debug)
                    printf("[0x%08x] : mftb r%d, spr%d\n", pc_buffer[pc_buffer_counter],
                           _get_field(insn, 6, 10), spr);
                assert(spr == 268 || spr == 269);
            }

            const u8 regD = _get_field(insn, 6, 10);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)regD);
            if (spr == SPR_XER) {
                curr_instruction = emit_load_u64(curr_instruction, 1, (u64)&cpu->state.xer);
                curr_instruction = emit_load_u32(curr_instruction, 2, 0);
            } else {
                curr_instruction =
                    emit_load_u64(curr_instruction, 1, (u64)&cpu->special_purpose_registers);

                curr_instruction = emit_load_u32(curr_instruction, 2, (u64)spr);
            }

            const u32 *main_block, *main_block_end;
            emit_mfspr(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_DCBF_EXT || _get_field(insn, 21, 30) == OPC_ICBI_EXT ||
            _get_field(insn, 21, 30) == OPC_DCBI_EXT) {
            // TODO: maybe simulate working cache
            u32 *curr_instruction = code_buffer;

            if (g_print_debug)
                printf("[0x%08x] : dcbf (NOP) \n", pc_buffer[pc_buffer_counter]);

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_MTSPR_EXT) {

            const u32 lo = _get_field(insn, 11, 15);
            const u32 hi = _get_field(insn, 16, 20);
            const u32 spr = (hi << 5) | lo;
            if (g_print_debug)
                printf("[0x%08x] : mtspr spr%d, r%d, \n", pc_buffer[pc_buffer_counter], spr,
                       _get_field(insn, 6, 10));

            const u8 regD = _get_field(insn, 6, 10);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)regD);
            if (spr == SPR_XER) {
                curr_instruction = emit_load_u64(curr_instruction, 1, (u64)&cpu->state.xer);
                curr_instruction = emit_load_u32(curr_instruction, 2, 0);
            } else {
                curr_instruction =
                    emit_load_u64(curr_instruction, 1, (u64)&cpu->special_purpose_registers);

                curr_instruction = emit_load_u32(
                    curr_instruction, 2,
                    spr == SPR_TBL_WRITE ? SPR_TBL : (spr == SPR_TBU_WRITE ? SPR_TBU : spr));
            }

            const u32 *main_block, *main_block_end;
            emit_mtspr(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;
            // TODO: check if the spr are part of the machine state and have to be terminating the
            // tb!!

            if (SPR_TERMINATING_INDEXES(spr)) {
                *termination_type = TERMINATING_TYPE_SPR_CHANGE;
            }
            return curr_instruction;
        }

        // TODO: read inner specifications and if everything matches the specifications
        if (_get_field(insn, 21, 30) == OPC_SYNC_EXT) {
            if (g_print_debug)
                printf("[0x%08x] : sync \n", pc_buffer[pc_buffer_counter]);
            u32 *curr_instruction = code_buffer;
            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_ORX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : or r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_orx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_NORX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : nor r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_norx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_ADDX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : add r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_addx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (oe == 1) {

                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }

        if (_get_field(insn, 21, 30) == OPC_CMP_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 cfd = _get_field(insn, 6, 8);
            const u32 L = _get_field(insn, 10, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : cmpi  r%d, r%d\n", pc_buffer[pc_buffer_counter], regA, regB);

            assert(L == 0);

            curr_instruction = emit_load_u32(curr_instruction, 0, cfd);
            curr_instruction = emit_load_u32(curr_instruction, 1, L);
            curr_instruction = emit_load_u32(curr_instruction, 2, regA);
            curr_instruction = emit_load_u32(curr_instruction, 3, regB);
            curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->state.cr);
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            const u32 *main_block, *main_block_end;
            emit_cmp(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
            break;
        }
        if (_get_field(insn, 22, 30) == OPC_ADDEX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : addex r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            u32 *main_block, *main_block_end;
            emit_addex(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            set_xer_ca_from_w15(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            if (oe == 1) {

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_ADDCX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : addcx r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_addx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            set_xer_ca_from_w15(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            if (oe == 1) {

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_ANDX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : and r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_andx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_ANDCX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : andc r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_andcx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_ORCX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : orc r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_orcx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_XORX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : xor r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_xorx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_NANDX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : nand r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_nandx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_EQVX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : eqv r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_eqvx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_SLWX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : slw r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_slwx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_SRWX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : srw r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_srwx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_SRAWX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : sraw r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_srawx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
            set_xer_ca_from_w15(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_SRAWIX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : srawi r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_srawix(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
            set_xer_ca_from_w15(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_CNTLZWX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : cntlzw r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_cntlzwx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_EXTSBX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : extsb r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_extsbx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_EXTSHX_EXT) {
            const u32 s = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : extsh r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], s, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, s);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_extshx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_CMPL_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 cfd = _get_field(insn, 6, 8);
            const u32 L = _get_field(insn, 10, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : cmpl  r%d, r%d\n", pc_buffer[pc_buffer_counter], regA, regB);

            assert(L == 0);

            curr_instruction = emit_load_u32(curr_instruction, 0, cfd);
            curr_instruction = emit_load_u32(curr_instruction, 1, L);
            curr_instruction = emit_load_u32(curr_instruction, 2, regA);
            curr_instruction = emit_load_u32(curr_instruction, 3, regB);
            curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->state.cr);
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            const u32 *main_block, *main_block_end;
            emit_cmpl(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
            break;
        }
        if (_get_field(insn, 22, 30) == OPC_SUBFX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : subf r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_subfx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (oe == 1) {

                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_NEGX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : neg r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_negx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (oe == 1) {

                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_MULLWX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : mullw r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_mullwx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (oe == 1) {

                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_DIVWX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : divw r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_divwx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (oe == 1) {

                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_DIVWUX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : divwu r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_divwux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (oe == 1) {

                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_MULHW_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : mulhw r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_mulhwx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_MULHWUX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : mulhwu r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_mulhwux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_SUBFCX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : subfc r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_subfx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            set_xer_ca_from_w15(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            if (oe == 1) {

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_SUBFEX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : subfe r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            u32 *main_block, *main_block_end;
            emit_subfex(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            set_xer_ca_from_w15(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            if (oe == 1) {

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_ADDZEX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : addze r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            u32 *main_block, *main_block_end;
            emit_addzex(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            set_xer_ca_from_w15(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            if (oe == 1) {

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_ADDMEX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : addme r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            u32 *main_block, *main_block_end;
            emit_addmex(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            set_xer_ca_from_w15(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            if (oe == 1) {

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_SUBFZEX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : subfze r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            u32 *main_block, *main_block_end;
            emit_subfzex(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            set_xer_ca_from_w15(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            if (oe == 1) {

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 22, 30) == OPC_SUBFMEX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            if (g_print_debug)
                printf("[0x%08x] : subfme r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            u32 *main_block, *main_block_end;
            emit_subfmex(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            set_xer_ca_from_w15(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            if (oe == 1) {

                set_xer_ov_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }
            if (rc == 1) {
                curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
                set_cr0_from_w15(&main_block, &main_block_end);
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }

        if (_get_field(insn, 21, 30) == OPC_LWZX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lwzx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_lwzx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LWZUX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lwzux r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_lwzux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STWX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stwx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_stwx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STWUX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stwux r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_stwux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LBZX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lbzx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_lbzx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LBZUX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lbzux r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            assert(regA != 0);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_lbzux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LHZX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lhzx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_lhzx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LHZUX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lhzux r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            assert(regA != 0);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_lhzux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LHAX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lhax r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_lhax(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LHAUX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lhaux r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            assert(regA != 0);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_lhaux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STBX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stbx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_stbx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STBUX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stbux r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            assert(regA != 0);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_stbux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STHX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : sthx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_sthx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STHUX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : sthux r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            assert(regA != 0);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_sthux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LWBRX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lwbrx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_lwbrx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LHBRX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lhbrx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_lhbrx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STWBRX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stwbrx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_stwbrx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STHBRX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : sthbrx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_sthbrx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LSWI_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 NB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lswi r%d, r%d, %d\n", pc_buffer[pc_buffer_counter], regD, regA,
                       NB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, NB == 0 ? 32 : NB);

            const u32 *main_block, *main_block_end;
            emit_lswi(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LSWX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lswx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->state.xer);

            const u32 *main_block, *main_block_end;
            emit_lswx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STSWI_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 NB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stswi r%d, r%d, %d\n", pc_buffer[pc_buffer_counter], regS, regA,
                       NB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, NB == 0 ? 32 : NB);

            const u32 *main_block, *main_block_end;
            emit_stswi(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STSWX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stswx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->state.xer);

            const u32 *main_block, *main_block_end;
            emit_stswx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LFDX_EXT) {
            *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
            u32 *curr_instruction = code_buffer;
            const u32 fD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : lfdx f%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], fD, regA,
                       regB);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->fpu.fpr[fD]);

            const u32 *main_block, *main_block_end;
            emit_lfdx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_set_ps0_from_gpr(curr_instruction, fD, 0);

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LFDUX_EXT) {
            *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
            u32 *curr_instruction = code_buffer;
            const u32 fD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : lfdux f%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], fD, regA,
                       regB);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->fpu.fpr[fD]);

            const u32 *main_block, *main_block_end;
            emit_lfdux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_set_ps0_from_gpr(curr_instruction, fD, 0);

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STFDX_EXT) {
            *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stfdx f%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 4, regS);

            const u32 *main_block, *main_block_end;
            emit_stfdx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STFDUX_EXT) {
            *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stfdux f%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 4, regS);

            const u32 *main_block, *main_block_end;
            emit_stfdux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LFSX_EXT) {
            *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
            u32 *curr_instruction = code_buffer;
            const u32 fD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : lfsx f%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], fD, regA,
                       regB);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->fpu.fpr[fD]);

            const u32 *main_block, *main_block_end;
            emit_lfsx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_set_ps0_from_gpr(curr_instruction, fD, 0);

            if (cpu->fpu.get_pse_bit(cpu) != 0)
                curr_instruction = emit_set_ps1_from_gpr(curr_instruction, fD, 0);

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LFSUX_EXT) {
            *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
            u32 *curr_instruction = code_buffer;
            const u32 fD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);

            if (g_print_debug)
                printf("[0x%08x] : lfsux f%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], fD, regA,
                       regB);

            assert(regA != 0);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->fpu.fpr[fD]);

            const u32 *main_block, *main_block_end;
            emit_lfsux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            curr_instruction = emit_set_ps0_from_gpr(curr_instruction, fD, 0);

            if (cpu->fpu.get_pse_bit(cpu) != 0)
                curr_instruction = emit_set_ps1_from_gpr(curr_instruction, fD, 0);

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STFSX_EXT) {
            *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stfsx f%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 4, regS);

            const u32 *main_block, *main_block_end;
            emit_stfsx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STFSUX_EXT) {
            *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stfsux f%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            assert(regA != 0);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 4, regS);

            const u32 *main_block, *main_block_end;
            emit_stfsux(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STFIWX_EXT) {
            *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stfiwx f%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 4, regS);

            const u32 *main_block, *main_block_end;
            emit_stfiwx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_LWARX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : lwarx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->reserve);

            const u32 *main_block, *main_block_end;
            emit_lwarx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_STWCX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : stwcx. r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);
            curr_instruction = emit_load_u64(curr_instruction, 3, (u64)&cpu->reserve);
            curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

            const u32 *main_block, *main_block_end;
            emit_stwcx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_ECIWX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : eciwx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regD,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regD);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_eciwx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_ECOWX_EXT) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : ecowx r%d, [r%d, r%d]\n", pc_buffer[pc_buffer_counter], regS,
                       regA, regB);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, regB);

            const u32 *main_block, *main_block_end;
            emit_ecowx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_MFSR_EXT) {
            const u32 reg = _get_field(insn, 6, 10);
            const u32 sr = _get_field(insn, 12, 15);
            if (g_print_debug)
                printf("[0x%08x] : mfsr r%d, %d\n", pc_buffer[pc_buffer_counter], reg, sr);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)reg);
            curr_instruction = emit_load_u64(curr_instruction, 1, (u64)&cpu->registers.sr);
            curr_instruction = emit_load_u32(curr_instruction, 2, (u64)sr);

            const u32 *main_block, *main_block_end;
            emit_mfspr(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_MTSR_EXT) {
            const u32 reg = _get_field(insn, 6, 10);
            const u32 sr = _get_field(insn, 12, 15);
            if (g_print_debug)
                printf("[0x%08x] : mtsr r%d, %d\n", pc_buffer[pc_buffer_counter], reg, sr);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)reg);
            curr_instruction = emit_load_u64(curr_instruction, 1, (u64)&cpu->registers.sr);
            curr_instruction = emit_load_u32(curr_instruction, 2, (u64)sr);

            const u32 *main_block, *main_block_end;
            emit_mtspr(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_MFSRIN_EXT) {
            const u32 reg = _get_field(insn, 6, 10);
            const u32 sr = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : mfsrin r%d, %d\n", pc_buffer[pc_buffer_counter], reg, sr);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)reg);
            curr_instruction = emit_load_u64(curr_instruction, 1, (u64)&cpu->registers.sr);
            curr_instruction = emit_load_u32(curr_instruction, 2, (u64)sr);

            const u32 *main_block, *main_block_end;
            emit_mfsrin(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_MTSRIN_EXT) {
            const u32 reg = _get_field(insn, 6, 10);
            const u32 sr = _get_field(insn, 16, 20);
            if (g_print_debug)
                printf("[0x%08x] : mtsrin r%d, %d\n", pc_buffer[pc_buffer_counter], reg, sr);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)reg);
            curr_instruction = emit_load_u64(curr_instruction, 1, (u64)&cpu->registers.sr);
            curr_instruction = emit_load_u32(curr_instruction, 2, (u64)sr);

            const u32 *main_block, *main_block_end;
            emit_mtsrin(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        printf("not implemented : %d\n", _get_field(insn, 21, 30));
        assert(!"not implemented guest instruction");
    }
    case OPC_STW: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : stw r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_stw(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_LHZ: {
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i32 d = _sign_extend(_get_field(insn, 16, 31), 16);
        if (g_print_debug)
            printf("[0x%08x] : lhz r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regD, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 0, regD);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, d);

        const u32 *main_block, *main_block_end;
        emit_lhz(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
    }
    case OPC_STH: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : sth r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_sth(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_STWU: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : stwu r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, d);

        assert(regA != 0);

        const u32 *main_block, *main_block_end;
        emit_stwu(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_STMW: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);

        const i32 simm = _sign_extend(_get_field(insn, 16, 31), 16);
        if (g_print_debug)
            printf("[0x%08x] : stmw r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA,
                   simm);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, simm);

        assert(regA != 0);

        const u32 *main_block, *main_block_end;
        emit_stmw(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_ORIS: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u16 uimm = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : oris r%d, r%d %u]\n", pc_buffer[pc_buffer_counter], regS, regA,
                   uimm);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)uimm << 16);

        const u32 *main_block, *main_block_end;
        emit_oris(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_LWZ: {
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : lwz r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regD, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 0, regD);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_lwz(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_STBU: {
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : stbu r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regD, regA, d);

        assert(regA != 0);

        curr_instruction = emit_load_u32(curr_instruction, 0, regD);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_stbu(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_LBZU: {
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : lbzu r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regD, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 0, regD);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_lbzu(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_LBZ: {
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : lbz r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regD, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 0, regD);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_lbz(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_LHZU: {
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : lhzu r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regD, regA, d);

        assert(regA != 0);

        curr_instruction = emit_load_u32(curr_instruction, 0, regD);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_lhzu(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_LHA: {
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : lha r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regD, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 0, regD);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_lha(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_LHAU: {
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : lhau r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regD, regA, d);

        assert(regA != 0);

        curr_instruction = emit_load_u32(curr_instruction, 0, regD);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_lhau(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_STB: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : stb r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_stb(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_STHU: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : sthu r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA, d);

        assert(regA != 0);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_sthu(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_RLWINM: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u32 SH = _get_field(insn, 16, 20);
        const u32 MB = _get_field(insn, 21, 25);
        const u32 ME = _get_field(insn, 26, 30);
        const u32 RC = _get_field(insn, 31, 31);
        if (g_print_debug)
            printf("[0x%08x] : rlwinm r%d, r%d, %d, %d, %d\n", pc_buffer[pc_buffer_counter], regS,
                   regA, SH, MB, ME);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, SH);
        curr_instruction = emit_load_u32(curr_instruction, 3, MB);
        curr_instruction = emit_load_u32(curr_instruction, 4, ME);
        curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);

        u32 *main_block, *main_block_end;
        emit_rlwinm(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        if (RC == 1) {
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
            emit_rlwinm_cr0_set(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
        }

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_CMPLI: {
        u32 *curr_instruction = code_buffer;
        const u32 cfd = _get_field(insn, 6, 8);
        const u32 L = _get_field(insn, 10, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u32 uimm = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : cmpli  r%d, %d\n", pc_buffer[pc_buffer_counter], regA, uimm);

        assert(L == 0);

        curr_instruction = emit_load_u32(curr_instruction, 0, cfd);
        curr_instruction = emit_load_u32(curr_instruction, 1, L);
        curr_instruction = emit_load_u32(curr_instruction, 2, regA);
        curr_instruction = emit_load_u32(curr_instruction, 3, uimm);
        curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->state.cr);
        curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

        const u32 *main_block, *main_block_end;
        emit_cpmli(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_STFD: {
        *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : stfd f%d, [f%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);
        curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 4, regS);

        const u32 *main_block, *main_block_end;
        emit_stfd(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_CMPI: {
        u32 *curr_instruction = code_buffer;
        const u32 cfd = _get_field(insn, 6, 8);
        const u32 L = _get_field(insn, 10, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i32 simm = _sign_extend(_get_field(insn, 16, 31), 16);
        if (g_print_debug)
            printf("[0x%08x] : cmpi  r%d, %d\n", pc_buffer[pc_buffer_counter], regA, simm);

        assert(L == 0);

        curr_instruction = emit_load_u32(curr_instruction, 0, cfd);
        curr_instruction = emit_load_u32(curr_instruction, 1, L);
        curr_instruction = emit_load_u32(curr_instruction, 2, regA);
        curr_instruction = emit_load_u32(curr_instruction, 3, simm);
        curr_instruction = emit_load_u64(curr_instruction, 4, (u64)&cpu->state.cr);
        curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

        const u32 *main_block, *main_block_end;
        emit_cmpi(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_SUBFIC: {

        const u32 rD_field = _get_field(insn, 6, 10);
        const u32 rA_field = _get_field(insn, 11, 15);

        const i32 imm = _sign_extend(_get_field(insn, 16, 31), 16);

        if (g_print_debug)
            printf("[0x%08x] : subfic r%d, r%d, imm(#%d / 0x%08x)\n", pc_buffer[pc_buffer_counter],
                   rD_field, rA_field, imm);
        u32 *curr = code_buffer;

        u32 *blk, *blk_end;
        curr = emit_load_u32(curr, 0, (u64)rA_field);
        curr = emit_load_u32(curr, 1, (u64)rD_field);
        curr = emit_load_u32(curr, 2, (i32)imm);

        emit_subfic(&blk, &blk_end);
        curr = write_to_buffer(curr, {blk, blk_end});

        curr = emit_load_u64(curr, 16, (u64)&cpu->state.xer);

        set_xer_ca_from_w15(&blk, &blk_end);
        curr = write_to_buffer(curr, {blk, blk_end});
        *pc_after_instruction += 4;
        return curr;
    }
    case OPC_MULLI: {

        const u32 rD_field = _get_field(insn, 6, 10);
        const u32 rA_field = _get_field(insn, 11, 15);

        const i32 imm = _sign_extend(_get_field(insn, 16, 31), 16);

        if (g_print_debug)
            printf("[0x%08x] : mulli r%d, r%d, imm(#%d / 0x%08x)\n", pc_buffer[pc_buffer_counter],
                   rD_field, rA_field, imm);
        u32 *curr = code_buffer;

        u32 *blk, *blk_end;
        curr = emit_load_u32(curr, 0, (u64)rA_field);
        curr = emit_load_u32(curr, 1, (u64)rD_field);
        curr = emit_load_u32(curr, 2, (i32)imm);

        emit_mulli(&blk, &blk_end);
        curr = write_to_buffer(curr, {blk, blk_end});
        *pc_after_instruction += 4;
        return curr;
    }
    case OPC_XORI: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u16 uimm = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : xori r%d, r%d %u]\n", pc_buffer[pc_buffer_counter], regS, regA,
                   uimm);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)uimm);

        u32 *main_block, *main_block_end;
        emit_xori(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_XORIS: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u16 uimm = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : xoris r%d, r%d %u]\n", pc_buffer[pc_buffer_counter], regS, regA,
                   uimm);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)uimm << 16);

        u32 *main_block, *main_block_end;
        emit_xori(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_ANDI: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u16 uimm = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : andi. r%d, r%d %u]\n", pc_buffer[pc_buffer_counter], regS, regA,
                   uimm);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)uimm);

        u32 *main_block, *main_block_end;
        emit_andi(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
        curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
        set_cr0_from_w15(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_ANDIS: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u16 uimm = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : andis. r%d, r%d %u]\n", pc_buffer[pc_buffer_counter], regS, regA,
                   uimm);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)uimm << 16);

        u32 *main_block, *main_block_end;
        emit_andi(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);
        curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
        set_cr0_from_w15(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_LWZU: {
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : lwzu r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regD, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 0, regD);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

        const u32 *main_block, *main_block_end;
        emit_lwzu(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_LMW: {
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);

        const i32 simm = _sign_extend(_get_field(insn, 16, 31), 16);
        if (g_print_debug)
            printf("[0x%08x] : lmw r%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regD, regA,
                   simm);

        curr_instruction = emit_load_u32(curr_instruction, 0, regD);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, simm);

        const u32 *main_block, *main_block_end;
        emit_lmw(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_RLWNM: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u32 SH = _get_field(insn, 16, 20);
        const u32 MB = _get_field(insn, 21, 25);
        const u32 ME = _get_field(insn, 26, 30);
        const u32 RC = _get_field(insn, 31, 31);
        if (g_print_debug)
            printf("[0x%08x] : rlwnm r%d, r%d, %d, %d, %d\n", pc_buffer[pc_buffer_counter], regS,
                   regA, SH, MB, ME);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, SH);
        curr_instruction = emit_load_u32(curr_instruction, 3, MB);
        curr_instruction = emit_load_u32(curr_instruction, 4, ME);
        curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);

        u32 *main_block, *main_block_end;
        emit_rlwnm(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        if (RC == 1) {
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
            emit_rlwinm_cr0_set(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
        }

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_RLWIMI: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u32 SH = _get_field(insn, 16, 20);
        const u32 MB = _get_field(insn, 21, 25);
        const u32 ME = _get_field(insn, 26, 30);
        const u32 RC = _get_field(insn, 31, 31);
        if (g_print_debug)
            printf("[0x%08x] : rlwimi r%d, r%d, %d, %d, %d\n", pc_buffer[pc_buffer_counter], regS,
                   regA, SH, MB, ME);

        curr_instruction = emit_load_u32(curr_instruction, 0, regS);
        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, SH);
        curr_instruction = emit_load_u32(curr_instruction, 3, MB);
        curr_instruction = emit_load_u32(curr_instruction, 4, ME);
        curr_instruction = emit_load_u64(curr_instruction, 5, (u64)&cpu->state.cr);

        u32 *main_block, *main_block_end;
        emit_rlwimi(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        if (RC == 1) {
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);
            emit_rlwinm_cr0_set(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
        }

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_STFDU: {
        *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : stfdu f%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA,
                   d);

        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);
        curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 4, regS);

        const u32 *main_block, *main_block_end;
        emit_stfdu(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_STFS: {
        *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : stfs f%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA, d);

        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);
        curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 4, regS);

        const u32 *main_block, *main_block_end;
        emit_stfs(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_STFSU: {
        *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
        if (g_print_debug)
            printf("[0x%08x] : stfsu f%d, [r%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA,
                   d);

        assert(regA != 0);

        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);
        curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 4, regS);

        const u32 *main_block, *main_block_end;
        emit_stfsu(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_PSQ_L: {
        *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
        u32 *curr_instruction = code_buffer;
        const u32 fD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u32 W = _get_field(insn, 16, 16);
        const u32 I = _get_field(insn, 17, 19);
        const i32 d = _sign_extend(_get_field(insn, 20, 31), 12);
        if (g_print_debug)
            printf("[0x%08x] : psq_l f%d, [r%d, %d], %d, qr%d\n", pc_buffer[pc_buffer_counter], fD,
                   regA, d, W, I);

        assert(cpu->fpu.get_pse_bit(cpu) != 0);

        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)d);
        curr_instruction = emit_load_u64(curr_instruction, 3,
                                         (u64)&cpu->special_purpose_registers.buf[SPR_GQR0 + I]);
        curr_instruction = emit_load_u32(curr_instruction, 4, W);

        const u32 *main_block, *main_block_end;
        emit_psq_l(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        curr_instruction = emit_set_ps0_from_gpr(curr_instruction, fD, 0);
        curr_instruction = emit_set_ps1_from_gpr(curr_instruction, fD, 1);

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_PSQ_LU: {
        *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
        u32 *curr_instruction = code_buffer;
        const u32 fD = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u32 W = _get_field(insn, 16, 16);
        const u32 I = _get_field(insn, 17, 19);
        const i32 d = _sign_extend(_get_field(insn, 20, 31), 12);
        if (g_print_debug)
            printf("[0x%08x] : psq_lu f%d, [r%d, %d], %d, qr%d\n", pc_buffer[pc_buffer_counter], fD,
                   regA, d, W, I);

        assert(cpu->fpu.get_pse_bit(cpu) != 0);
        assert(regA != 0);

        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)d);
        curr_instruction = emit_load_u64(curr_instruction, 3,
                                         (u64)&cpu->special_purpose_registers.buf[SPR_GQR0 + I]);
        curr_instruction = emit_load_u32(curr_instruction, 4, W);

        const u32 *main_block, *main_block_end;
        emit_psq_lu(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        curr_instruction = emit_set_ps0_from_gpr(curr_instruction, fD, 0);
        curr_instruction = emit_set_ps1_from_gpr(curr_instruction, fD, 1);

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_PSQ_ST: {
        *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
        u32 *curr_instruction = code_buffer;
        const u32 fS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u32 W = _get_field(insn, 16, 16);
        const u32 I = _get_field(insn, 17, 19);
        const i32 d = _sign_extend(_get_field(insn, 20, 31), 12);
        if (g_print_debug)
            printf("[0x%08x] : psq_st f%d, [r%d, %d], %d, qr%d\n", pc_buffer[pc_buffer_counter], fS,
                   regA, d, W, I);

        assert(cpu->fpu.get_pse_bit(cpu) != 0);

        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)d);
        curr_instruction = emit_load_u64(curr_instruction, 3,
                                         (u64)&cpu->special_purpose_registers.buf[SPR_GQR0 + I]);
        curr_instruction = emit_load_u32(curr_instruction, 4, W);
        curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 5, fS);
        curr_instruction = emit_get_ps1_into_gpr(curr_instruction, 6, fS);

        const u32 *main_block, *main_block_end;
        emit_psq_st(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case OPC_PSQ_STU: {
        *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;
        u32 *curr_instruction = code_buffer;
        const u32 fS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u32 W = _get_field(insn, 16, 16);
        const u32 I = _get_field(insn, 17, 19);
        const i32 d = _sign_extend(_get_field(insn, 20, 31), 12);
        if (g_print_debug)
            printf("[0x%08x] : psq_stu f%d, [r%d, %d], %d, qr%d\n", pc_buffer[pc_buffer_counter],
                   fS, regA, d, W, I);

        assert(cpu->fpu.get_pse_bit(cpu) != 0);
        assert(regA != 0);

        curr_instruction = emit_load_u32(curr_instruction, 1, regA);
        curr_instruction = emit_load_u32(curr_instruction, 2, (u32)d);
        curr_instruction = emit_load_u64(curr_instruction, 3,
                                         (u64)&cpu->special_purpose_registers.buf[SPR_GQR0 + I]);
        curr_instruction = emit_load_u32(curr_instruction, 4, W);
        curr_instruction = emit_fmov_into_gpr_64(curr_instruction, 5, fS);
        curr_instruction = emit_get_ps1_into_gpr(curr_instruction, 6, fS);

        const u32 *main_block, *main_block_end;
        emit_psq_stu(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    // TODO: add interrupt
    case OPC_PS_NEG: {
        *tb_type |= TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS;

        if (_get_field(insn, 25, 30) == OPC_PSQ_LX_EXT ||
            _get_field(insn, 25, 30) == OPC_PSQ_LUX_EXT) {
            const bool update = _get_field(insn, 25, 30) == OPC_PSQ_LUX_EXT;
            u32 *curr = code_buffer;
            const u32 fD = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            const u32 W = _get_field(insn, 21, 21);
            const u32 I = _get_field(insn, 22, 24);
            if (g_print_debug)
                printf("[0x%08x] : psq_l%sx f%d, [r%d, r%d], %d, qr%d\n",
                       pc_buffer[pc_buffer_counter], update ? "u" : "", fD, regA, regB, W, I);

            assert(cpu->fpu.get_pse_bit(cpu) != 0);
            if (update)
                assert(regA != 0);

            curr = emit_load_u32(curr, 1, regA);
            curr = emit_load_u32(curr, 2, regB);
            curr = emit_load_u64(curr, 3, (u64)&cpu->special_purpose_registers.buf[SPR_GQR0 + I]);
            curr = emit_load_u32(curr, 4, W);

            const u32 *main_block, *main_block_end;
            if (update)
                emit_psq_lux(&main_block, &main_block_end);
            else
                emit_psq_lx(&main_block, &main_block_end);
            curr = write_to_buffer(curr, {main_block, main_block_end});

            curr = emit_set_ps0_from_gpr(curr, fD, 0);
            curr = emit_set_ps1_from_gpr(curr, fD, 1);

            *pc_after_instruction += 4;

            return curr;
        }

        if (_get_field(insn, 25, 30) == OPC_PSQ_STX_EXT ||
            _get_field(insn, 25, 30) == OPC_PSQ_STUX_EXT) {
            const bool update = _get_field(insn, 25, 30) == OPC_PSQ_STUX_EXT;
            u32 *curr = code_buffer;
            const u32 fS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const u32 regB = _get_field(insn, 16, 20);
            const u32 W = _get_field(insn, 21, 21);
            const u32 I = _get_field(insn, 22, 24);
            if (g_print_debug)
                printf("[0x%08x] : psq_st%sx f%d, [r%d, r%d], %d, qr%d\n",
                       pc_buffer[pc_buffer_counter], update ? "u" : "", fS, regA, regB, W, I);

            assert(cpu->fpu.get_pse_bit(cpu) != 0);
            if (update)
                assert(regA != 0);

            curr = emit_load_u32(curr, 1, regA);
            curr = emit_load_u32(curr, 2, regB);
            curr = emit_load_u64(curr, 3, (u64)&cpu->special_purpose_registers.buf[SPR_GQR0 + I]);
            curr = emit_load_u32(curr, 4, W);
            curr = emit_fmov_into_gpr_64(curr, 5, fS);
            curr = emit_get_ps1_into_gpr(curr, 6, fS);

            const u32 *main_block, *main_block_end;
            if (update)
                emit_psq_stux(&main_block, &main_block_end);
            else
                emit_psq_stx(&main_block, &main_block_end);
            curr = write_to_buffer(curr, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr;
        }

        assert(_get_field(insn, 21, 30) == OPC_PS_NEG_EXT);
        u32 *curr_instruction = code_buffer;
        const u32 regD = _get_field(insn, 6, 10);
        const u32 regB = _get_field(insn, 16, 20);
        const u32 rc = _get_field(insn, 31, 31);
        if (g_print_debug)
            printf("[0x%08x] : ps_neg f%d, f%d\n", pc_buffer[pc_buffer_counter], regD, regB);

        // TODO: if this happens envoke exception
        assert(cpu->fpu.get_pse_bit(cpu) != 0);

        curr_instruction = emit_fneg_ps(curr_instruction, regD, regB);

        if (rc == 1) {
            curr_instruction = emit_load_u64(curr_instruction, 15, (u64)&cpu->state.cr);
            curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.fpscr);

            u32 *blk, *blk_end;
            copy_fpscr_to_cr1(&blk, &blk_end);
            curr_instruction = write_to_buffer(curr_instruction, {blk, blk_end});
        }

        *pc_after_instruction += 4;

        return curr_instruction;
        break;
    }
    case SYSTEM_CALL_OPCODE: {
        u32 *curr_instruction = code_buffer;
        if (g_print_debug)
            printf("[0x%08x] : sc\n", pc_buffer[pc_buffer_counter]);
        printf("SYSCALL at : 0x%08x\n", pc_buffer[pc_buffer_counter]);

        curr_instruction = emit_load_u32(curr_instruction, 0, pc_buffer[pc_buffer_counter]);

        const u32 *main_block, *main_block_end;
        emit_sc(&main_block, &main_block_end);
        curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

        *pc_after_instruction += 4;

        *termination_type = TERMINATING_TYPE_RET;

        return curr_instruction;
        break;
    }
    default:

        printf("insn : 0x%08x\n", insn);
        assert(!"not implemented guest instruction");
    }
}

static u32 *emit_pc_store(u32 pc_to_push, u32 *code_buffer, CPU *cpu)
{
    if (g_print_debug)
        printf("pc store emit\n");
    u32 *curr_instruction = code_buffer;
    const u32 *pc_register_ptr = &cpu->state.pc;

    curr_instruction = emit_load_u64(curr_instruction, 0, (u64)pc_register_ptr);
    curr_instruction = emit_load_u32(curr_instruction, 1, (i32)pc_to_push);
    curr_instruction = emit_store_u32(curr_instruction, 1, 0, 0);

    return curr_instruction;
}
static u32 *emit_prologue(u32 *out, const CPU *cpu)
{
    out = emit_load_u64(out, GUEST_REGISTER_POINTER, (u64)cpu->registers.gpio);
    out = emit_load_u64(out, GUEST_HELPER_FUNCTIONS_POINTER, (u64)cpu->helper_functions);
    return out;
}

static u32 *emit_nop(u32 *out)
{
    out[0] = 0xD503201F;
    return &out[1];
}

// TODO: add variable sizing for the code blocks
bool tb_translate(CPU *cpu, CpuMode cpu_mode, TranslationBlock *out_tb, bool print_debug)
{
    g_print_debug = print_debug;

    const u32 pc_at_start = cpu->state.pc;

    if (g_print_debug)
        printf("pc at start : 0x%08x\n", pc_at_start);

    CodeBuffer cb;
    if (!code_buffer_init(&cb, TB_INITIAL_CAPACITY)) {
        return false;
    }

    u32 *out = (u32 *)cb.code;

    // for init calls in the future
    out = emit_nop(out);
    out = emit_nop(out);
    out = emit_nop(out);

    out = emit_prologue(out, cpu);

    cb.size = (u32)((u8 *)out - cb.code);

    u32 pc_buffer[MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK];
    u32 *host_block_buffer[MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK];
    u16 pc_count = 0;

    u32 pc = cpu->state.pc;
    u8 termination_type = 0;

    // for future optimization
    //(only load required registers)
    FPRUsageBitmap fpr_bitmap = 0;

    while (termination_type == 0 && pc_count < MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK) {

        if (!code_buffer_reserve(&cb, TB_MAX_BYTES_PER_GUEST_INSTRUCTION + TB_EPILOGUE_MAX_BYTES)) {
            if (g_print_debug)
                printf("tb full after %u guest instructions\n", pc_count);
            break;
        }

        out = (u32 *)(cb.code + cb.size);

        const u32 guest_instruction = (u32)cpu->bus->read(cpu->bus, pc, 4);
        if (guest_instruction == 0) {
            if (g_print_debug)
                printf("fetch fault at 0x%08x\n", pc);
            code_buffer_destroy(&cb);

            if (g_print_debug == false)
                tb_translate(cpu, cpu_mode, out_tb, true);

            assert(!"guest instruction 0 \n");
            return false;
        }

        if (g_print_debug)
            printf("current pc: 0x%08x\n", pc);
        pc_buffer[pc_count] = pc;
        host_block_buffer[pc_count] = out;

        u32 *block_start = out;
        out = _translate_instruction(guest_instruction, cpu, &pc, pc_buffer, host_block_buffer,
                                     pc_count, out, &termination_type, &out_tb->type, &fpr_bitmap);
        pc_count += 1;

        if (g_print_debug)
            printf("code size : %llu\n", (u64)((u8 *)out - (u8 *)block_start));
        assert((u64)((u8 *)out - (u8 *)block_start) <= TB_MAX_BYTES_PER_GUEST_INSTRUCTION);
        if (g_print_debug)
            _print_code_block(block_start, (u32)(out - block_start));

        cb.size = (u32)((u8 *)out - cb.code);
    }

    if (!code_buffer_reserve(&cb, TB_EPILOGUE_MAX_BYTES)) {
        code_buffer_destroy(&cb);

        assert(!"code buffer reserve\n");
        return false;
    }
    out = (u32 *)(cb.code + cb.size);

    if (termination_type != TERMINATING_TYPE_RET) {
        out = emit_pc_store(pc, out, cpu);
    }

    *out++ = HOST_INSTRUCTION_RET;
    cb.size = (u32)((u8 *)out - cb.code);

    if (!code_buffer_make_executable(&cb)) {
        code_buffer_destroy(&cb);
        assert(!"code buffer make exec\n");
        return false;
    }

    if (g_print_debug)
        _print_code_block((u32 *)cb.code, (cb.size / 4) + 0xa000);

    if (g_print_debug)
        printf("adr of ret : 0x%016llx\n", ((u64)out) - 4);

    if (g_print_debug)
        printf("pc at end : 0x%08x\n", pc_at_start);

    const TranslationBlockCore core = {.code = cb.code, cb.size};

    *out_tb = (TranslationBlock){
        .core = core,
        .pc_at_start = pc_at_start,
        .msr_at_start = cpu->state.msr,
        .hid2_at_start = cpu->special_purpose_registers.hid2,
        .type = out_tb->type,
    };
    return true;
}
