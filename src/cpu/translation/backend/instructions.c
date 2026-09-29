#include "instructions.h"
#include <oaknut_enc.h>
#include <stddef.h>
#include "../../cpu_types.h"
#include "core/config/config.h"

static inline u32 _get_field(u32 insn, u8 start, u8 end)
{
    u32 width = end - start + 1;
    return (insn >> (31 - end)) & ((1u << width) - 1u);
}
static inline i32 _sign_extend(u32 v, u8 bits)
{
    const u32 m = 1u << (bits - 1);
    return (i32)((v ^ m) - m);
}

static inline bool save_host_reg(const u8 reg_num)
{
    return reg_num > 17;
}

// moves guest reg to higher half of xn -> n is idx
static u32 *push_guest(TranslateFuncContext ctx, const u8 src_reg, const u8 idx)
{
    *ctx.code_buffer++ = oak_enc_MOV_w_w(idx, idx);
    *ctx.code_buffer++ = oak_enc_LSL_x_x_imm(src_reg, src_reg, 32);
    *ctx.code_buffer++ = oak_enc_ORR_x_x_x(idx, idx, src_reg);

    return ctx.code_buffer;
}

// pops higher half of xn -> n is idx into the  dest wn reg
static u32 *pop_guest(TranslateFuncContext ctx, const u8 dest, const u8 idx)
{
    *ctx.code_buffer++ = oak_enc_LSR_w_w_imm(dest, idx, 32);
    return ctx.code_buffer;
}

// str guest regs to to gpio array of cpu and load host regs for func call
static u32 *context_switch_to_host(TranslateFuncContext ctx)
{

    if (*ctx.reg_usage_bit_map & 1) {
        *ctx.code_buffer++ = oak_enc_MOV_w_w(1, 1);
        *ctx.code_buffer++ = oak_enc_LSL_x_x_imm(0, 0, 32);
        *ctx.code_buffer++ = oak_enc_ORR_x_x_x(1, 1, 0);
    }

    *ctx.code_buffer++ = oak_enc_MOVZ_x_imm(0, ((u64)static_cpu_ptr) & 0x000000000000FFFFull);
    *ctx.code_buffer++ = oak_enc_MOVK_x_imm(0, ((u64)static_cpu_ptr) & 0x00000000FFFF0000ull);
    *ctx.code_buffer++ = oak_enc_MOVK_x_imm(0, ((u64)static_cpu_ptr) & 0x0000FFFF00000000ull);
    *ctx.code_buffer++ = oak_enc_MOVK_x_imm(0, ((u64)static_cpu_ptr) & 0xFFFF000000000000ull);
    for (int i = 31; i > 0; i++) {
        if ((*ctx.reg_usage_bit_map >> i) & 1) {
            if (i == 0) {
                *ctx.code_buffer++ = oak_enc_LSR_x_x_imm(1, 1, 32);
                *ctx.code_buffer++ = oak_enc_STR_w_xsp_imm(1, 0, offsetof(CPU, registers));
            } else {
                *ctx.code_buffer++ =
                    oak_enc_STR_w_xsp_imm(i, 0, offsetof(CPU, registers) + (i * 4));
                if (i > 17)
                    *ctx.code_buffer++ = oak_enc_LDR_w_xsp_imm(i, 0, offsetof(CPU, host) + (i * 4));
            }
        }
    }
    return ctx.code_buffer;
}
static u32 *context_switch_to_guest(TranslateFuncContext ctx, const u32 ignore_regs_bitmap)
{
    // not enough space
    if ((ctx.code_buffer_end - ctx.code_buffer) < (70 * 4))
        return NULL_PTR;

    *ctx.code_buffer++ = oak_enc_MOVZ_x_imm(9, ((u64)static_cpu_ptr) & 0x000000000000FFFFull);
    *ctx.code_buffer++ = oak_enc_MOVK_x_imm(9, ((u64)static_cpu_ptr) & 0x00000000FFFF0000ull);
    *ctx.code_buffer++ = oak_enc_MOVK_x_imm(9, ((u64)static_cpu_ptr) & 0x0000FFFF00000000ull);
    *ctx.code_buffer++ = oak_enc_MOVK_x_imm(9, ((u64)static_cpu_ptr) & 0xFFFF000000000000ull);

    for (int i = 19; i < 32; i++) {
        *ctx.code_buffer++ = oak_enc_STR_x_xsp_imm(i, 0, offsetof(CPU, host) + (i * 8));
    }
    for (int i = 31; i > 0; i++) {
        if ((((*ctx.reg_usage_bit_map) & ~ignore_regs_bitmap) >> i) & 1) {
            if (i == 0) {
                *ctx.code_buffer++ = oak_enc_LSR_x_x_imm(1, 1, 32);
                *ctx.code_buffer++ = oak_enc_LDR_w_xsp_imm(1, 0, offsetof(CPU, host) + (i * 4));
            } else {

                *ctx.code_buffer++ =
                    oak_enc_LDR_w_xsp_imm(i, 0, offsetof(CPU, registers) + (i * 4));
            }
        }
    }
    return ctx.code_buffer;
}

u32 *translate_addi_func(TranslateFuncContext ctx)
{
    const u32 rD_field = _get_field(ctx.instruction, 6, 10);
    const u32 rA_field = _get_field(ctx.instruction, 11, 15);
    const i32 imm = _sign_extend(_get_field(ctx.instruction, 16, 31), 16);

    *ctx.reg_usage_bit_map |= rD_field;
    *ctx.reg_usage_bit_map |= rA_field;

    if (rA_field == 0) {
        *ctx.code_buffer++ = oak_enc_MOVK_w_imm(rD_field, imm);
    } else {
        *ctx.code_buffer++ = oak_enc_MOV_w_w(rD_field, rA_field);
        *ctx.code_buffer++ = oak_enc_ADDS_w_wsp_imm(rD_field, rD_field, imm);
    }

    return ctx.code_buffer;
}
u32 *translate_lwz_func(TranslateFuncContext ctx)
{
    const u32 rD_field = _get_field(ctx.instruction, 6, 10);
    const u32 rA_field = _get_field(ctx.instruction, 11, 15);
    const i32 d = _sign_extend(_get_field(ctx.instruction, 16, 31), 16);

    *ctx.reg_usage_bit_map |= rD_field;
    *ctx.reg_usage_bit_map |= rA_field;

    ctx.code_buffer = push_guest(ctx, 0, 2);
    if (rA_field == 0) {
        *ctx.code_buffer++ = oak_enc_MOVK_w_imm(0, 0);
    } else {
        *ctx.code_buffer++ = oak_enc_MOV_w_w(0, rA_field);
    }

    ctx.code_buffer = push_guest(ctx, 1, 3);
    *ctx.code_buffer++ = oak_enc_MOVK_w_imm(1, d);
    *ctx.code_buffer++ = oak_enc_ADD_w_w_w(0, 0, 1);
    ctx.code_buffer = pop_guest(ctx, 1, 3);

    // adr is in w0
    ctx.code_buffer = context_switch_to_host(ctx);

    *ctx.code_buffer++ =
        oak_enc_MOVZ_x_imm(9, ((u64)_helper_read_word_from_bus) & 0x000000000000FFFFull);
    *ctx.code_buffer++ =
        oak_enc_MOVK_x_imm(9, ((u64)_helper_read_word_from_bus) & 0x00000000FFFF0000ull);
    *ctx.code_buffer++ =
        oak_enc_MOVK_x_imm(9, ((u64)_helper_read_word_from_bus) & 0x0000FFFF00000000ull);
    *ctx.code_buffer++ =
        oak_enc_MOVK_x_imm(9, ((u64)_helper_read_word_from_bus) & 0xFFFF000000000000ull);
    *ctx.code_buffer++ = oak_enc_BLR_x(9);

    // result is in w0

    *ctx.code_buffer++ = oak_enc_MOV_w_w(rD_field, 0);
    return context_switch_to_guest(ctx, 1 << rD_field);
}
