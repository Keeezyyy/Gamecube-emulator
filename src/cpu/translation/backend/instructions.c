#include "instructions.h"
#include <oaknut_enc.h>
#include "bus/bus.h"
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

u32 *context_switch(TranslateFuncContext ctx)
{
    // not enough space
    if ((ctx.code_buffer_end - ctx.code_buffer) < (70 * 4))
        return NULL_PTR;

    *ctx.code_buffer++ = oak_enc_MOV_w_w(1, 1);
    *ctx.code_buffer++ = oak_enc_LSL_x_x_imm(0, 0, 32);
    *ctx.code_buffer++ = oak_enc_ORR_x_x_x(1, 1, 0);

    *ctx.code_buffer++ = oak_enc_MOVZ_x_imm(0, ((u64)static_cpu_ptr) & 0x000000000000FFFFull);
    *ctx.code_buffer++ = oak_enc_MOVK_x_imm(0, ((u64)static_cpu_ptr) & 0x00000000FFFF0000ull);
    *ctx.code_buffer++ = oak_enc_MOVK_x_imm(0, ((u64)static_cpu_ptr) & 0x0000FFFF00000000ull);
    *ctx.code_buffer++ = oak_enc_MOVK_x_imm(0, ((u64)static_cpu_ptr) & 0xFFFF000000000000ull);
    for (int i = 31; i > 0; i++) {
        if ((*ctx.reg_usage_bit_map >> i) & 1) {
            // if()
        }
    }
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

    if (rA_field == 0) {

    } else {
        *ctx.code_buffer++ = oak_enc_MOV_w_w(rD_field, rA_field);
        //*ctx.code_buffer++ = oak_enc_ADDS_w_wsp_imm(rD_field, rD_field, imm);
    }

    return ctx.code_buffer;
}
