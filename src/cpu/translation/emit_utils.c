#include "emit_utils.h"

static uint64_t ror(uint64_t x, unsigned r, unsigned width)
{
    r %= width;

    uint64_t mask = (1ULL << width) - 1;
    x &= mask;

    return ((x >> r) | (x << (width - r))) & mask;
}
static bool is_low_ones(uint64_t x, unsigned width)
{
    uint64_t mask = (1ULL << width) - 1;
    x &= mask;

    return x != 0 && ((x & (x + 1)) == 0);
}
static unsigned popcount64(uint64_t x)
{
    unsigned count = 0;

    while (x) {
        count += x & 1;
        x >>= 1;
    }

    return count;
}
bool encode_logical_imm(uint64_t mask, uint32_t *N, uint32_t *immr, uint32_t *imms)
{
    if (mask == 0 || mask == UINT64_MAX)
        return false;

    for (unsigned len = 1; len <= 6; len++) {

        unsigned esize = 1u << len;

        uint64_t elem = mask & ((1ULL << esize) - 1);

        uint64_t repeated = 0;

        for (unsigned pos = 0; pos < 64; pos += esize)
            repeated |= elem << pos;

        if (repeated != mask)
            continue;
        for (unsigned r = 0; r < esize; r++) {

            uint64_t rotated = ror(elem, r, esize);

            if (is_low_ones(rotated, esize)) {

                unsigned ones = popcount64(rotated);

                unsigned s = ones - 1;

                *N = (len == 6) ? 1 : 0;

                *immr = r;
                unsigned levels = (1u << len) - 1;

                *imms = ((~levels) & 0x3f) | s;

                return true;
            }
        }
    }

    return false;
}

// ORR(IMM) rever algo :
static inline int ctz64(uint64_t x)
{
    return x ? __builtin_ctzll(x) : 64;
}
static inline int clz64(uint64_t x)
{
    return x ? __builtin_clzll(x) : 64;
}
static inline int cto64(uint64_t x)
{
    return ctz64(~x);
} /* count trailing ones  */
static inline int clo64(uint64_t x)
{
    return clz64(~x);
} /* count leading ones   */

/* x == 0...01...1  (Maske ab Bit 0) */
static inline bool is_mask64(uint64_t x)
{
    return x && ((x + 1) & x) == 0;
}

/* x == 0...01...10...0  (ein zusammenhaengender Block von Einsen) */
static inline bool is_shifted_mask64(uint64_t x)
{
    return x && is_mask64((x - 1) | x);
}

/* n Einsen, n in [0,64] */
static inline uint64_t ones64(int n)
{
    return n >= 64 ? ~UINT64_C(0) : (UINT64_C(1) << n) - 1;
}

/* ------------------------------------------------------------------ */
/* ENCODE  (die gesuchte Umkehrung)                                    */
/* ------------------------------------------------------------------ */
/*
 * regsize muss 32 oder 64 sein.
 * Rueckgabe: true, wenn der Wert als Logical Immediate darstellbar ist.
 *
 * Ein Wert ist genau dann kodierbar, wenn er aus einem Element der Groesse
 * esize in {2,4,8,16,32,64} besteht, das ueber regsize repliziert wird, und
 * dieses Element (zyklisch betrachtet) genau einen zusammenhaengenden Block
 * von Einsen enthaelt -- weder alles 0 noch alles 1.
 */
bool a64_encode_bitmask(uint64_t imm, int regsize, a64_logical_imm *out)
{
    if (regsize != 32 && regsize != 64)
        return false;

    /* Bei 32 Bit: obere Haelfte muss leer sein, dann auf 64 Bit replizieren,
     * damit wir einheitlich mit 64-Bit-Arithmetik arbeiten koennen. */
    if (regsize == 32) {
        if (imm >> 32)
            return false;
        imm |= imm << 32;
    }

    /* all-zeros und all-ones sind nicht kodierbar */
    if (imm == 0 || imm == ~UINT64_C(0))
        return false;

    /* --- 1) Elementgroesse esize bestimmen: kleinste Periode des Musters --- */
    int size = 64;
    do {
        size /= 2;
        uint64_t mask = ones64(size);
        if ((imm & mask) != ((imm >> size) & mask)) {
            size *= 2; /* auf dieser Groesse bricht die Periode */
            break;
        }
    } while (size > 2);

    /* --- 2) Rotation bestimmen, so dass das Element die Form 0^m 1^n hat --- */
    uint64_t mask = ~UINT64_C(0) >> (64 - size);
    uint64_t elem = imm & mask;

    int rot; /* Anzahl der RORs, um vom Zielwert nach 0^m 1^n zu kommen */
    int cto; /* Anzahl der Einsen im Block  (= s + 1) */

    if (is_shifted_mask64(elem)) {
        /* Block liegt komplett innerhalb des Elements: 0^a 1^n 0^b */
        rot = ctz64(elem);
        cto = cto64(elem >> rot);
    } else {
        /* Block laeuft ueber den Elementrand ("wrap around"): 1^a 0^m 1^b.
         * Obere Bits mit Einsen auffuellen, dann muss das Komplement ein
         * zusammenhaengender Block sein. */
        elem |= ~mask;
        if (!is_shifted_mask64(~elem))
            return false;

        int clo = clo64(elem);
        rot = 64 - clo;
        cto = clo + cto64(elem) - (64 - size);
    }

    /* immr = Anzahl der Rechtsrotationen von 0^m 1^n zurueck zum Zielwert */
    uint32_t immr = (uint32_t)((size - rot) & (size - 1));

    /* --- 3) imms bauen: 1-Bits oberhalb von log2(esize), darunter s = n-1 --- */
    uint32_t nimms = (uint32_t)(~(size - 1) << 1);
    nimms |= (uint32_t)(cto - 1);

    /* Bit 6 von nimms invertiert ergibt N (esize == 64  ->  N == 1) */
    uint32_t N = ((nimms >> 6) & 1) ^ 1;

    out->N = N;
    out->immr = immr;
    out->imms = nimms & 0x3f;
    return true;
}

/* ------------------------------------------------------------------ */
/* DECODE  (Referenz, 1:1 nach ARM-Pseudocode)                         */
/* ------------------------------------------------------------------ */

bool a64_decode_bitmasks(uint32_t N, uint32_t imms, uint32_t immr, bool immediate, int M,
                         uint64_t *wmask, uint64_t *tmask)
{
    imms &= 0x3f;
    immr &= 0x3f;

    /* len = HighestSetBit(N : NOT(imms)) ueber 7 Bits */
    uint32_t combined = ((N & 1) << 6) | (~imms & 0x3f);
    if (combined == 0)
        return false; /* UNDEFINED */
    int len = 31 - __builtin_clz(combined);
    if (len < 1)
        return false; /* UNDEFINED */
    if (M < (1 << len))
        return false; /* assert    */

    uint32_t levels = (uint32_t)ones64(len);

    if (immediate && (imms & levels) == levels)
        return false; /* UNDEFINED */

    int s = (int)(imms & levels);
    int r = (int)(immr & levels);
    int diff = (s - r) & 0x3f; /* 6-Bit-Subtraktion mit Borrow */

    int esize = 1 << len;
    int d = diff & (int)levels;

    uint64_t welem = ones64(s + 1);
    uint64_t telem = ones64(d + 1);

    /* ROR(welem, r) innerhalb von esize */
    uint64_t emask = ~UINT64_C(0) >> (64 - esize);
    uint64_t w = r ? (((welem >> r) | (welem << (esize - r))) & emask) : welem;

    /* Replicate(..., M DIV esize) */
    uint64_t wm = 0, tm = 0;
    for (int i = 0; i < M; i += esize) {
        wm |= w << i;
        tm |= telem << i;
    }
    if (M < 64) {
        wm &= ones64(M);
        tm &= ones64(M);
    }

    if (wmask)
        *wmask = wm;
    if (tmask)
        *tmask = tm;
    return true;
}
