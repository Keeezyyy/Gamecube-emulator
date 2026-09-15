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

#define CORRECT_ENDIAN(x) (_endian32(x, false))

#define TERMINATING_TYPE_CONDITINIAL_BRANCH 1
#define TERMINATING_TYPE_MSR_CHANGE 2
#define TERMINATING_TYPE_SPR_CHANGE 3

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

        u32 *curr_instruction = code_buffer;
        if (LK) {
            cpu->special_purpose_registers.lr = pc_buffer[pc_buffer_counter] + 4;

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
            }
        } else if (_get_field(insn, 21, 30) == OPC_ISYNC_EXT) {
            printf("[0x%08x] : isync \n", pc_buffer[pc_buffer_counter]);
            u32 *curr_instruction = code_buffer;
            *pc_after_instruction += 4;

            return curr_instruction;
        } else if (_get_field(insn, 21, 30) == OPC_CRXOR_EXT) {
            const u32 crbD = _get_field(insn, 6, 10);
            const u32 crbA = _get_field(insn, 11, 15);
            const u32 crbB = _get_field(insn, 16, 20);

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
        } else {
            abort();
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
        // TODO: check manual for deeper functions and if it changes machine context
        // TODO: check manual for deeper functions and if it changes machine context
        // TODO: check manual for deeper functions and if it changes machine context
        // TODO: check manual for deeper functions and if it changes machine context
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
            const u32 lo = _get_field(insn, 11, 15);
            const u32 hi = _get_field(insn, 16, 20);
            const u32 spr = (hi << 5) | lo;
            printf("[0x%08x] : mfspr r%d, spr%d\n", pc_buffer[pc_buffer_counter],
                   _get_field(insn, 6, 10), spr);

            const u8 regD = _get_field(insn, 6, 10);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)regD);
            curr_instruction =
                emit_load_u64(curr_instruction, 1, (u64)&cpu->special_purpose_registers);

            curr_instruction = emit_load_u32(curr_instruction, 2, (u64)spr);

            const u32 *main_block, *main_block_end;
            emit_mfspr(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_MTSPR_EXT) {
            const u32 lo = _get_field(insn, 11, 15);
            const u32 hi = _get_field(insn, 16, 20);
            const u32 spr = (hi << 5) | lo;
            printf("[0x%08x] : mtspr spr%d, r%d, \n", pc_buffer[pc_buffer_counter], spr,
                   _get_field(insn, 6, 10));

            const u8 regD = _get_field(insn, 6, 10);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, (u64)regD);
            curr_instruction =
                emit_load_u64(curr_instruction, 1, (u64)&cpu->special_purpose_registers);

            curr_instruction = emit_load_u32(curr_instruction, 2, (u64)spr);

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
                curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});
            }

            *pc_after_instruction += 4;

            return curr_instruction;
        }
        if (_get_field(insn, 21, 30) == OPC_ADDX_EXT) {
            const u32 d = _get_field(insn, 6, 10);
            const u32 a = _get_field(insn, 11, 15);
            const u32 b = _get_field(insn, 16, 20);
            const u32 oe = _get_field(insn, 21, 21);
            const u32 rc = _get_field(insn, 31, 31);

            printf("[0x%08x] : add r%d, r%d, r%d \n", pc_buffer[pc_buffer_counter], d, a, b);

            u32 *curr_instruction = code_buffer;

            curr_instruction = emit_load_u32(curr_instruction, 0, d);
            curr_instruction = emit_load_u32(curr_instruction, 1, a);
            curr_instruction = emit_load_u32(curr_instruction, 2, b);

            u32 *main_block, *main_block_end;
            emit_addx(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            if (rc == 1) {

                curr_instruction = emit_load_u64(curr_instruction, 16, (u64)&cpu->state.xer);

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
        printf("not implemented : %d\n", _get_field(insn, 21, 30));
        assert(!"not implemented guest instruction");
    }
    case OPC_STW: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
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
    case OPC_STWU: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const i16 d = _get_field(insn, 16, 31);
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
    case OPC_ORIS: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u16 uimm = _get_field(insn, 16, 31);
        printf("[0x%08x] : oris r%d, r%d %u]\n", pc_buffer[pc_buffer_counter], regS, regA, uimm);

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
    case OPC_RLWINM: {
        u32 *curr_instruction = code_buffer;
        const u32 regS = _get_field(insn, 6, 10);
        const u32 regA = _get_field(insn, 11, 15);
        const u32 SH = _get_field(insn, 16, 20);
        const u32 MB = _get_field(insn, 21, 25);
        const u32 ME = _get_field(insn, 26, 30);
        const u32 RC = _get_field(insn, 31, 31);
        printf("[0x%08x] : rlwinm r%d, r%d, %d, %d, %d\n", pc_buffer[pc_buffer_counter], regS, regA,
               SH, MB, ME);

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
        abort();
        if (cpu->fpu.get_pse_bit(cpu) == 0) {
            u32 *curr_instruction = code_buffer;
            const u32 regS = _get_field(insn, 6, 10);
            const u32 regA = _get_field(insn, 11, 15);
            const i16 d = _get_field(insn, 16, 31);
            printf("[0x%08x] : stfd f%d, [f%d, %d]\n", pc_buffer[pc_buffer_counter], regS, regA, d);

            curr_instruction = emit_load_u32(curr_instruction, 0, regS);
            curr_instruction = emit_load_u32(curr_instruction, 1, regA);
            curr_instruction = emit_load_u32(curr_instruction, 2, (i32)d);

            const u32 *main_block, *main_block_end;
            emit_stw(&main_block, &main_block_end);
            curr_instruction = write_to_buffer(curr_instruction, {main_block, main_block_end});

            *pc_after_instruction += 4;

            return curr_instruction;
            break;
        } else {
            abort();
        }
    }
    default:

        assert(!"not implemented guest instruction");
    }
}

static u32 *emit_pc_store(u32 pc_to_push, u32 *code_buffer, CPU *cpu)
{
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

// TODO: add variable sizing for the code blocks
bool tb_translate(CPU *cpu, CpuMode cpu_mode, TranslationBlock *out_tb)
{
    const u32 pc_at_start = cpu->state.pc;

    printf("pc at start : 0x%08x\n", pc_at_start);

    CodeBuffer cb;
    if (!code_buffer_init(&cb, TB_INITIAL_CAPACITY)) {
        return false;
    }

    u32 *out = emit_prologue((u32 *)cb.code, cpu);
    cb.size = (u32)((u8 *)out - cb.code);

    u32 pc_buffer[MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK];
    u16 pc_count = 0;

    u32 pc = cpu->state.pc;
    u8 termination_type = 0;

    while (termination_type == 0 && pc_count < MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK) {

        if (!code_buffer_reserve(&cb, TB_MAX_BYTES_PER_GUEST_INSTRUCTION + TB_EPILOGUE_MAX_BYTES)) {
            TB_TRACE("tb full after %u guest instructions\n", pc_count);
            break;
        }

        out = (u32 *)(cb.code + cb.size);

        const u32 *guest_instruction = cpu->bus->read(cpu->bus, pc);
        if (guest_instruction == NULL) {
            TB_TRACE("fetch fault at 0x%08x\n", pc);
            code_buffer_destroy(&cb);
            return false;
        }

        TB_TRACE("current pc: 0x%08x\n", pc);
        pc_buffer[pc_count] = pc;

        u32 *block_start = out;
        out = _translate_instruction(CORRECT_ENDIAN(*guest_instruction), cpu, &pc, pc_buffer,
                                     pc_count, out, &termination_type);
        pc_count += 1;

        printf("code size : %llu\n", (u64)((u8 *)out - (u8 *)block_start));
        assert((u64)((u8 *)out - (u8 *)block_start) <= TB_MAX_BYTES_PER_GUEST_INSTRUCTION);
        TB_TRACE_CODE(block_start, (u32)(out - block_start));

        cb.size = (u32)((u8 *)out - cb.code);
    }

    if (!code_buffer_reserve(&cb, TB_EPILOGUE_MAX_BYTES)) {
        code_buffer_destroy(&cb);
        return false;
    }
    out = (u32 *)(cb.code + cb.size);

    out = emit_pc_store(pc, out, cpu);

    *out++ = HOST_INSTRUCTION_RET;
    cb.size = (u32)((u8 *)out - cb.code);

    if (!code_buffer_make_executable(&cb)) {
        code_buffer_destroy(&cb);
        return false;
    }

    TB_TRACE_CODE((u32 *)cb.code, cb.size / 4);

    printf("adr of ret : 0x%016llx\n", ((u64)out) - 4);

    const TranslationBlockCore core = {.code = cb.code, cb.size};

    *out_tb = (TranslationBlock){
        .core = core,
        .pc_at_start = pc_at_start,
        .msr_at_start = cpu->state.msr,
        .hid2_at_start = cpu->special_purpose_registers.hid2,
    };
    return true;
}
