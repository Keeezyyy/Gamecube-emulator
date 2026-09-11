#include "emit.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/emit_utils.h"
#include "cpu/translation/translation_core_defines.h"
#include <assert.h>
#include <stdbool.h>

#include <stdio.h>

static inline void _write_instruction_to_buffer(EmitedBlock *eb, u32 insn,
                                                HostInstructionsBlock *buffer,
                                                u32 *host_instruction_counter, u64 id)
{
    buffer->host_code_buffer[buffer->num_of_instructions++] = insn;
    buffer->id = id;
}

static u8 phys_reg(u8 r)
{
    return (r < GUEST_MIN) ? r : (u8)((r - GUEST_MIN) / 2);
}

void emit_cbz_cbnz(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter,
                   u64 id, u8 r1, bool is64, bool branch_on_zero, u64 host_instruction_block_id)
{
    // https://finkmartin.com/aarch64/cbz.html
    u32 insn = 0;
    if (is64) {
        insn |= BIT(31);
    }
    if (r1 % 2 == 0 || r1 < GUEST_MIN) {
        // host register
        insn |= 0b0011010u << 25; // sf=0, op=0, fixed bits

        if (branch_on_zero) {
            insn |= BIT(24);
        }
        insn |= r1 & 0x1F;

        // NOTE: the offset is in 4 byte jumps
        insn |= ((0 >> 2) & 0x7FFFF) << 5;

        EmitPatch p;
        p.emitted_block_id = host_instruction_block_id;
        p.bit_start = 5;
        p.bit_end = 23;
        p.bit_mask = 0x7FFFF;
        p.bit_shift = 5;
        p.instruction_ptr = buffer->host_code_buffer;

        eb->patch_ptr[eb->num_of_patches++] = p;

        _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
        return;
    } else {
        // NOTE: cb on guest registers only support 32 bit mode
        //  lsr x16, xr, #32
        //  cbz x16
        emit_lsr(eb, buffer, host_instruction_counter, id, GUEST_TO_HOST_CONVERSION_ACCUMILATOR,
                 ((r1 - 32) - 1) / 2, false, 32);
        emit_cbz_cbnz(eb, buffer, host_instruction_counter, id, r1, is64, branch_on_zero,
                      host_instruction_block_id);
    }
}

void emit_str(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rn, u8 rt, bool is64, str_mode mode, i32 imm)
{
    // STR Wt/Xt, [Xn|SP], #imm
    // https://finkmartin.com/aarch64/str_imm_gen.html

    printf("imm : %i\n", imm);
    assert(rn < GUEST_MIN || rt < GUEST_MIN);

    u32 insn = 0;
    insn |= (is64 ? 0b11u : 0b10u) << 30;

    switch (mode) {
    case STR_POST_INDEX:
        insn |= 0b111000u << 24;

        assert(imm >= -256 && imm <= 255);
        insn |= (imm & 0x1FFu) << 12;

        insn |= 0b01u << 10;
        break;

    case STR_PRE_INDEX:
        insn |= 0b111000u << 24;

        assert(imm >= -256 && imm <= 255);
        insn |= ((u32)imm & 0x1FFu) << 12;

        insn |= 0b11u << 10;
        break;

    case STR_UNSIGNED_OFFSET: {
        printf("unsigned offset \n");
        insn |= 0b111001u << 24;

        u32 scale = is64 ? 8 : 4;

        assert(imm >= 0);
        assert((u32)imm % scale == 0);

        u32 imm12 = (u32)imm / scale;
        assert(imm12 <= 0xFFF);

        insn |= imm12 << 10;
        break;
    }

    default:
        printf("some error\n");
        assert(!"invalid STR mode");
    }

    insn |= ((u32)rn & 0x1Fu) << 5;

    insn |= (u32)rt & 0x1Fu;

    printf("instruction xxx: 0b");
    for (int i = 31; i >= 0; i--) {
        printf("%d", (insn >> i) & 1);
    }
    printf("\n");

    _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
}

void emit_lsr(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rd, u8 rn, bool is64, u32 shift)
{

    u32 insn;

    assert(shift < (is64 ? 64u : 32u));

    if (is64) {
        insn = 0xD340FC00u;
        insn |= ((u32)(shift & 0x3F)) << 16;
    } else {
        insn = 0x53007C00u;
        insn |= ((u32)(shift & 0x1F)) << 16;
    }

    insn |= ((u32)(rn & 0x1F)) << 5;
    insn |= ((u32)(rd & 0x1F));

    _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
}
void emit_asr(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rd, u8 rn, bool is64, u8 shift)
{

    u32 insn;

    assert(shift < (is64 ? 64u : 32u));

    insn = is64 ? 0x9340FC00u : 0x13007C00u;

    insn |= ((u32)shift) << 16;
    insn |= ((u32)(rn & 0x1F)) << 5;
    insn |= ((u32)(rd & 0x1F));

    _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
}
void emit_movz(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter,
               u64 id, u8 reg, u16 imm, u8 type, bool is64)
{

    if (reg < GUEST_MIN) {
        u32 insn = 0;
        if (is64)
            insn |= BIT(31);
        assert(is64 || (type & 0b10) == 0);
        insn |= 0b10100101u << 23;
        insn |= ((u32)type & 0b11u) << 21;
        insn |= ((u32)imm) << 5;
        insn |= (reg & 0x1Fu);
        _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
        return;
    }

    assert(!is64);
    assert((type & 0b10) == 0);

    u8 host = phys_reg(reg);
    u8 lsb = (u8)((reg % 2 == 0 ? 0 : 32) + type * 16);

    emit_movz(eb, buffer, host_instruction_counter, id, GUEST_TO_HOST_CONVERSION_ACCUMILATOR, imm,
              SHIFT_TYPE_0, false);
    emit_bfi(eb, buffer, host_instruction_counter, id, host, GUEST_TO_HOST_CONVERSION_ACCUMILATOR,
             true, lsb, 16);
}

void emit_mov(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rd, u8 rs, bool is64)
{

    u32 insn = 0;
    if (is64) {
        insn |= BIT(31);
    }
    insn |= 0b0101010000 << 21;
    insn |= 0b00000011111 << 5;
    // for cases:
    // -> GUEST TO HOST
    // -> HOST TO GUEST
    if ((rs > HOST_MAX && rd < GUEST_MIN) || rs <= HOST_MAX && rd >= GUEST_MIN) {
        assert(!is64);
        // GUEST TO HOST OP CANNOT USE 64 bit reg

        if (rs % 2 == 0) {
            // GUEST reg can be accessed with wx host register

            insn |= (0x3F & rs - 32) << 16; // source
            insn |= (0x3F & rd);

            _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
            return;

        } else if ((rs > HOST_MAX && rd < GUEST_MIN)) {
            // GUEST TO HOST (guest reg is in upper half)

            emit_asr(eb, buffer, host_instruction_counter, id, GUEST_TO_HOST_CONVERSION_ACCUMILATOR,
                     ((rs - 32) - 1) / 2, true, 32);

            insn |= (0x3F & GUEST_TO_HOST_CONVERSION_ACCUMILATOR - 32) << 16;
            insn |= (0x3F & rd);

            _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
            return;
        }
    }

    // -> GUEST TO GUEST
    if (rs > HOST_MAX || rd > HOST_MAX) {
        assert(!is64);

        if (rs % 2 == 0 && rd % 2 == 0) {   // -> both in Wx registers
            insn |= (0x3F & rs - 32) << 16; // source
            insn |= (0x3F & rd - 32);

        } else if (rs % 2 == 0 && rd % 2 != 0) { // -> rd is a upper reg

            emit_bfi(eb, buffer, host_instruction_counter, id, phys_reg(rd), phys_reg(rs), true, 32,
                     32);
        } else if (rs % 2 != 0 && rd % 2 == 0) { // -> rs is a upper reg

            emit_asr(eb, buffer, host_instruction_counter, id, GUEST_TO_HOST_CONVERSION_ACCUMILATOR,
                     ((rs - 32) - 1) / 2, true, 32);
            insn |= (0x3F & GUEST_TO_HOST_CONVERSION_ACCUMILATOR - 32) << 16;
            insn |= (0x3F & rd);

            _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
            // TODO:
        } else { // both are upper registers

            emit_asr(eb, buffer, host_instruction_counter, id, GUEST_TO_HOST_CONVERSION_ACCUMILATOR,
                     phys_reg(rs), true, 32);
            emit_bfi(eb, buffer, host_instruction_counter, id, phys_reg(rd),
                     GUEST_TO_HOST_CONVERSION_ACCUMILATOR, true, 32, 32);
        }

        return;
    } else {

        // -> HOST TO HOST
        insn |= (0x3F & rs) << 16; // source
        insn |= (0x3F & rd);
    }

    _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
    return;
}

void emit_movk(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter,
               u64 id, u8 reg, u16 imm, u8 type, bool is64)
{
    printf("imm : 0x%04x\n", imm);

    u32 insn = 0;

    if (reg < GUEST_MIN) {
        assert(is64 || (type & 0b10) == 0);

        if (is64) {
            insn |= BIT(31);
        }
        insn |= 0b11100101u << 23; // opc = 11 -> MOVK
        insn |= ((u32)type & 0b11u) << 21;
        insn |= ((u32)imm) << 5;
        insn |= ((u32)reg & 0x1Fu);

        _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
        return;
    }

    assert(!is64);
    assert((type & 0b10) == 0);

    u8 host = (u8)((reg - GUEST_MIN) / 2);
    u8 hw = (reg % 2 == 0) ? type : (u8)(type + 2);

    insn |= BIT(31);
    insn |= 0b11100101u << 23;
    insn |= ((u32)hw & 0b11u) << 21;
    insn |= ((u32)imm) << 5;
    insn |= ((u32)host & 0x1Fu);

    _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
}
void emit_bfi(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rd, u8 rn, bool is64, u8 lsb, u8 width)
{

    u32 insn;
    u32 datasize = is64 ? 64u : 32u;

    assert(width >= 1);
    assert((u32)lsb + (u32)width <= datasize);

    u32 immr = ((u32)(datasize - lsb)) % datasize;
    u32 imms = (u32)width - 1u;

    insn = is64 ? 0xB3400000u : 0x33000000u;
    insn |= (immr & 0x3Fu) << 16;
    insn |= (imms & 0x3Fu) << 10;
    insn |= ((u32)(rn & 0x1Fu)) << 5;
    insn |= ((u32)(rd & 0x1Fu));

    _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
}
void emit_ldr(EmitedBlock *eb, HostInstructionsBlock *buffer, u32 *host_instruction_counter, u64 id,
              u8 rn, u8 rt, bool is64, ldr_mode mode, i32 imm)
{

    assert(rn < GUEST_MIN);

    if (rt >= GUEST_MIN) {
        assert(!is64);

        emit_ldr(eb, buffer, host_instruction_counter, id, rn, GUEST_TO_HOST_CONVERSION_ACCUMILATOR,
                 false, mode, imm);
        emit_bfi(eb, buffer, host_instruction_counter, id, phys_reg(rt),
                 GUEST_TO_HOST_CONVERSION_ACCUMILATOR, true, (rt % 2 == 0) ? 0 : 32, 32);
        return;
    }

    u32 insn = 0;
    insn |= (is64 ? 0b11u : 0b10u) << 30;
    insn |= BIT(22);
    switch (mode) {
    case LDR_POST_INDEX:
        insn |= 0b111000u << 24;

        assert(imm >= -256 && imm <= 255);
        insn |= ((u32)imm & 0x1FFu) << 12;
        insn |= 0b01u << 10;
        break;

    case LDR_PRE_INDEX:
        insn |= 0b111000u << 24;

        assert(imm >= -256 && imm <= 255);
        insn |= ((u32)imm & 0x1FFu) << 12;
        insn |= 0b11u << 10;
        break;

    case LDR_UNSIGNED_OFFSET: {
        insn |= 0b111001u << 24;

        u32 scale = is64 ? 8u : 4u;

        assert(imm >= 0);
        assert((u32)imm % scale == 0);

        u32 imm12 = (u32)imm / scale;
        assert(imm12 <= 0xFFF);

        insn |= imm12 << 10;
        break;
    }

    default:
        assert(!"invalid LDR mode");
    }

    insn |= ((u32)rn & 0x1Fu) << 5;
    insn |= ((u32)rt & 0x1Fu);

    _write_instruction_to_buffer(eb, insn, buffer, host_instruction_counter, id);
}
