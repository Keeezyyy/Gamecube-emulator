#include "emit.h"
#include "cpu/translation/translation_core_defines.h"
#include "cpu/translation/translation_core_macros.h"
#include <assert.h>

inline void emit_cbz(EmitedBlock *eb, u32 *counter, u8 r1, u32 offset)
{

    // https://finkmartin.com/aarch64/cbz.html
    if (r1 < GUEST_MIN) {
        // r1 is in a host register
        u32 insn = 0;
        insn |= 0b00110100u << 24; // sf=0, op=0, fixed bits
        insn |= ((offset >> 2) & 0x7FFFF) << 5;
        insn |= r1 & 0x1F;
        out[*counter++] = insn;
        return;
    } else {
        // emit_mov(out, counter, GUEST_REG(r1), );
    }
}

inline void emit_str(EmitedBlock *eb, u32 *counter, u8 rn, u8 rt, bool is64, str_mode mode, i32 imm)
{
    // STR Wt/Xt, [Xn|SP], #imm
    // https://finkmartin.com/aarch64/str_imm_gen.html

    assert(rn < GUEST_MIN || rt < GUEST_MIN);

    u32 insn = 0;
    insn |= (is64 ? 0b11u : 0b10u) << 30;

    switch (mode) {
    case STR_POST_INDEX:
        insn |= 0b111000u << 23;

        assert(imm >= -256 && imm <= 255);
        insn |= ((u32)imm & 0x1FFu) << 12;

        insn |= 0b01u << 10;
        break;

    case STR_PRE_INDEX:
        insn |= 0b111000u << 23;

        assert(imm >= -256 && imm <= 255);
        insn |= ((u32)imm & 0x1FFu) << 12;

        insn |= 0b11u << 10;
        break;

    case STR_UNSIGNED_OFFSET: {
        insn |= 0b111001u << 23;

        u32 scale = is64 ? 8 : 4;

        assert(imm >= 0);
        assert((u32)imm % scale == 0);

        u32 imm12 = (u32)imm / scale;
        assert(imm12 <= 0xFFF);

        insn |= imm12 << 10;
        break;
    }

    default:
        assert(!"invalid STR mode");
    }

    insn |= ((u32)rn & 0x1Fu) << 5;

    insn |= (u32)rt & 0x1Fu;

    out[(*counter)++] = insn;
}
