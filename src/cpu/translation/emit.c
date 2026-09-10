#include "emit.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/translation_core_defines.h"
#include <assert.h>
#include <stdio.h>

void emit_cbz(EmitedBlock *eb, u8 r1, u16 index_of_offset_emitted_block)
{

    // https://finkmartin.com/aarch64/cbz.html
    if (r1 % 2 == 0) {
        // host register
        u32 insn = 0;
        insn |= 0b00110100u << 24; // sf=0, op=0, fixed bits
        insn |= r1 & 0x1F;

        // NOTE: the offset is in 4 byte jumps
        insn |= ((0x1000 >> 2) & 0x7FFFF) << 5;

        EmitPatch p;
        p.emitted_block_index = index_of_offset_emitted_block;
        p.bit_start = 5;
        p.bit_end = 23;
        p.bit_mask = 0x7FFFF;
        p.bit_shift = 5;
        p.instruction = &eb->block[eb->size];

        eb->patch_ptr[eb->num_of_patches++] = p;

        eb->block[eb->size++] = insn;
        return;
    } else {
        // lsr x16, xr, #32
        // cbz x16
        emit_lsr(eb, 16, ((r1 - 32) - 1) / 2, true, 32);
        emit_cbz(eb, 16, index_of_offset_emitted_block);
    }
}

void emit_str(EmitedBlock *eb, u8 rn, u8 rt, bool is64, str_mode mode, i32 imm)
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

    eb->block[eb->size++] = insn;
}

void emit_lsr(EmitedBlock *eb, u8 rd, u8 rn, bool is64, u8 immr)
{
    u32 insn = 0x53000000u; /* UBFM Wd, Wn, #immr, #imms */
    u32 imms = 31;

    if (is64) {
        insn |= (1u << 31); /* sf */
        insn |= (1u << 22); /* N  */
        imms = 63;
    }

    insn |= ((u32)(immr & 0x3F)) << 16;
    insn |= (imms & 0x3F) << 10;
    insn |= ((u32)(rn & 0x1F)) << 5;
    insn |= ((u32)(rd & 0x1F)) << 0;

    eb->block[eb->size++] = insn;
}
