#pragma STDC FP_CONTRACT OFF

#include "cpu/fpu.h"
#include <math.h>
#include <stdbool.h>

__extension__ typedef unsigned __int128 u128;

#define F64_SIGN 0x8000000000000000ull
#define F64_EXP 0x7ff0000000000000ull
#define F64_FRAC 0x000fffffffffffffull
#define F64_QUIET 0x0008000000000000ull
#define F64_ONE 0x3ff0000000000000ull
#define F64_DEFAULT_QNAN 0x7ff8000000000000ull
#define F64_SINGLE_NAN_MASK 0xffffffffe0000000ull
#define F64_INT_HIGH_WORD 0xfff8000000000000ull

#define FPSCR_VX_ALL                                                                               \
    (FPSCR_VXSNAN | FPSCR_VXISI | FPSCR_VXIDI | FPSCR_VXZDZ | FPSCR_VXIMZ | FPSCR_VXVC |           \
     FPSCR_VXSOFT | FPSCR_VXSQRT | FPSCR_VXCVI)
#define FPSCR_EXCEPTIONS (FPSCR_OX | FPSCR_UX | FPSCR_ZX | FPSCR_XX | FPSCR_VX_ALL)

#define FPRF_QNAN 0x11u
#define FPRF_NEG_INF 0x09u
#define FPRF_NEG_NORMAL 0x08u
#define FPRF_NEG_DENORMAL 0x18u
#define FPRF_NEG_ZERO 0x12u
#define FPRF_POS_ZERO 0x02u
#define FPRF_POS_DENORMAL 0x14u
#define FPRF_POS_NORMAL 0x04u
#define FPRF_POS_INF 0x05u

enum {
    PREC_SINGLE,
    PREC_DOUBLE,
};

typedef struct {
    u32 bits;
    i32 emin;
    i32 emax;
    i32 bias_adjust;
    u64 max_finite;
    u32 min_normal_exp;
} Precision;

static const Precision PRECISIONS[2] = {
    {24, -126, 127, 192, 0x47efffffe0000000ull, 897},
    {53, -1022, 1023, 1536, 0x7fefffffffffffffull, 1},
};

enum {
    ARITH_ADD,
    ARITH_SUB,
    ARITH_MUL,
    ARITH_DIV,
    ARITH_MADD,
    ARITH_MSUB,
    ARITH_NMADD,
    ARITH_NMSUB,
    ARITH_RES,
    ARITH_RSQRTE,
    ARITH_RSP,
};

enum {
    FRAC_ZERO,
    FRAC_BELOW_HALF,
    FRAC_HALF,
    FRAC_ABOVE_HALF,
};

typedef enum {
    FP_UNKNOWN,
    FP_FADDS,
    FP_FSUBS,
    FP_FMULS,
    FP_FDIVS,
    FP_FRES,
    FP_FMADDS,
    FP_FMSUBS,
    FP_FNMADDS,
    FP_FNMSUBS,
    FP_FADD,
    FP_FSUB,
    FP_FMUL,
    FP_FDIV,
    FP_FSEL,
    FP_FRSQRTE,
    FP_FMADD,
    FP_FMSUB,
    FP_FNMADD,
    FP_FNMSUB,
    FP_FCMPU,
    FP_FCMPO,
    FP_FRSP,
    FP_FCTIW,
    FP_FCTIWZ,
    FP_MFFS,
    FP_MCRFS,
    FP_MTFSB0,
    FP_MTFSB1,
    FP_MTFSFI,
    FP_MTFSF,
    FP_PS_ADD,
    FP_PS_SUB,
    FP_PS_MUL,
    FP_PS_DIV,
    FP_PS_MADD,
    FP_PS_MSUB,
    FP_PS_NMADD,
    FP_PS_NMSUB,
    FP_PS_MULS0,
    FP_PS_MULS1,
    FP_PS_MADDS0,
    FP_PS_MADDS1,
    FP_PS_SUM0,
    FP_PS_SUM1,
    FP_PS_RES,
    FP_PS_RSQRTE,
    FP_PS_SEL,
    FP_PS_MR,
    FP_PS_ABS,
    FP_PS_NABS,
    FP_PS_MERGE00,
    FP_PS_MERGE01,
    FP_PS_MERGE10,
    FP_PS_MERGE11,
    FP_PS_CMPU0,
    FP_PS_CMPO0,
    FP_PS_CMPU1,
    FP_PS_CMPO1,
    FP_COUNT,
} FpInsn;

static const char *const MNEMONICS[FP_COUNT] = {
    [FP_FADDS] = "fadds",         [FP_FSUBS] = "fsubs",         [FP_FMULS] = "fmuls",
    [FP_FDIVS] = "fdivs",         [FP_FRES] = "fres",           [FP_FMADDS] = "fmadds",
    [FP_FMSUBS] = "fmsubs",       [FP_FNMADDS] = "fnmadds",     [FP_FNMSUBS] = "fnmsubs",
    [FP_FADD] = "fadd",           [FP_FSUB] = "fsub",           [FP_FMUL] = "fmul",
    [FP_FDIV] = "fdiv",           [FP_FSEL] = "fsel",           [FP_FRSQRTE] = "frsqrte",
    [FP_FMADD] = "fmadd",         [FP_FMSUB] = "fmsub",         [FP_FNMADD] = "fnmadd",
    [FP_FNMSUB] = "fnmsub",       [FP_FCMPU] = "fcmpu",         [FP_FCMPO] = "fcmpo",
    [FP_FRSP] = "frsp",           [FP_FCTIW] = "fctiw",         [FP_FCTIWZ] = "fctiwz",
    [FP_MFFS] = "mffs",           [FP_MCRFS] = "mcrfs",         [FP_MTFSB0] = "mtfsb0",
    [FP_MTFSB1] = "mtfsb1",       [FP_MTFSFI] = "mtfsfi",       [FP_MTFSF] = "mtfsf",
    [FP_PS_ADD] = "ps_add",       [FP_PS_SUB] = "ps_sub",       [FP_PS_MUL] = "ps_mul",
    [FP_PS_DIV] = "ps_div",       [FP_PS_MADD] = "ps_madd",     [FP_PS_MSUB] = "ps_msub",
    [FP_PS_NMADD] = "ps_nmadd",   [FP_PS_NMSUB] = "ps_nmsub",   [FP_PS_MULS0] = "ps_muls0",
    [FP_PS_MULS1] = "ps_muls1",   [FP_PS_MADDS0] = "ps_madds0", [FP_PS_MADDS1] = "ps_madds1",
    [FP_PS_SUM0] = "ps_sum0",     [FP_PS_SUM1] = "ps_sum1",     [FP_PS_RES] = "ps_res",
    [FP_PS_RSQRTE] = "ps_rsqrte", [FP_PS_SEL] = "ps_sel",       [FP_PS_MR] = "ps_mr",
    [FP_PS_ABS] = "ps_abs",       [FP_PS_NABS] = "ps_nabs",     [FP_PS_MERGE00] = "ps_merge00",
    [FP_PS_MERGE01] = "ps_merge01", [FP_PS_MERGE10] = "ps_merge10",
    [FP_PS_MERGE11] = "ps_merge11", [FP_PS_CMPU0] = "ps_cmpu0", [FP_PS_CMPO0] = "ps_cmpo0",
    [FP_PS_CMPU1] = "ps_cmpu1",   [FP_PS_CMPO1] = "ps_cmpo1",
};

typedef struct {
    bool sign;
    i32 exp;
    u128 sig;
} Unpacked;

typedef struct {
    u64 value;
    u32 exc;
    u32 frfi;
    bool invalid;
    bool zero_divide;
} LaneResult;

static u64 _bits(double v)
{
    union {
        double d;
        u64 u;
    } c = {.d = v};
    return c.u;
}

static double _value(u64 b)
{
    union {
        u64 u;
        double d;
    } c = {.u = b};
    return c.d;
}

static u32 _exp_field(u64 b)
{
    return (u32)((b >> 52) & 0x7ffu);
}

static bool _is_nan(u64 b)
{
    return (b & ~F64_SIGN) > F64_EXP;
}

static bool _is_snan(u64 b)
{
    return _is_nan(b) && (b & F64_QUIET) == 0;
}

static bool _is_inf(u64 b)
{
    return (b & ~F64_SIGN) == F64_EXP;
}

static bool _is_zero(u64 b)
{
    return (b & ~F64_SIGN) == 0;
}

static bool _is_neg(u64 b)
{
    return (b & F64_SIGN) != 0;
}

static bool _is_denormal(u64 b, int prec)
{
    return !_is_zero(b) && _exp_field(b) < PRECISIONS[prec].min_normal_exp;
}

static u64 _flush(u64 b, int prec, u32 fpscr)
{
    if ((fpscr & FPSCR_NI) != 0 && _is_denormal(b, prec))
        return b & F64_SIGN;
    return b;
}

static u64 _quiet(u64 b, int prec)
{
    const u64 q = b | F64_QUIET;
    return prec == PREC_SINGLE ? q & F64_SINGLE_NAN_MASK : q;
}

static u64 _cancel_zero(u32 fpscr)
{
    return (fpscr & FPSCR_RN_MASK) == 3u ? F64_SIGN : 0;
}

static u32 _msb128(u128 v)
{
    const u64 hi = (u64)(v >> 64);
    if (hi != 0)
        return 127u - (u32)__builtin_clzll(hi);
    return 63u - (u32)__builtin_clzll((u64)v);
}

static u128 _shift_right_jam(u128 v, u32 d)
{
    if (d == 0)
        return v;
    if (d >= 128)
        return v != 0;
    return (v >> d) | (u128)((v & (((u128)1 << d) - 1)) != 0);
}

static Unpacked _unpack(u64 b)
{
    Unpacked u = {.sign = _is_neg(b), .exp = -1074, .sig = b & F64_FRAC};
    const u32 e = _exp_field(b);
    if (e != 0) {
        u.sig |= (u128)1 << 52;
        u.exp = (i32)e - 1075;
    }
    return u;
}

static Unpacked _normalize(Unpacked u, u32 top)
{
    const u32 m = _msb128(u.sig);
    u.sig <<= top - m;
    u.exp -= (i32)(top - m);
    return u;
}

static Unpacked _mul_exact(Unpacked x, Unpacked y)
{
    return (Unpacked){.sign = x.sign != y.sign, .exp = x.exp + y.exp, .sig = x.sig * y.sig};
}

static Unpacked _add_exact(Unpacked x, Unpacked y)
{
    x = _normalize(x, 125);
    y = _normalize(y, 125);
    if (x.exp < y.exp) {
        const Unpacked t = x;
        x = y;
        y = t;
    }
    y.sig = _shift_right_jam(y.sig, (u32)(x.exp - y.exp));

    Unpacked r = {.sign = x.sign, .exp = x.exp, .sig = 0};
    if (x.sign == y.sign) {
        r.sig = x.sig + y.sig;
    } else if (x.sig >= y.sig) {
        r.sig = x.sig - y.sig;
    } else {
        r.sig = y.sig - x.sig;
        r.sign = y.sign;
    }
    return r;
}

static Unpacked _div_exact(Unpacked x, Unpacked y)
{
    x = _normalize(x, 52);
    y = _normalize(y, 52);
    const u128 n = x.sig << 75;
    u128 q = n / y.sig;
    if (n % y.sig != 0)
        q |= 1;
    return (Unpacked){.sign = x.sign != y.sign, .exp = x.exp - 75 - y.exp, .sig = q};
}

static u64 _round(Unpacked u, int prec, u32 fpscr, LaneResult *r)
{
    const Precision *p = &PRECISIONS[prec];
    const u32 rn = fpscr & FPSCR_RN_MASK;
    const u64 sign = u.sign ? F64_SIGN : 0;
    const i32 e = (i32)_msb128(u.sig) + u.exp;
    const bool tiny = e < p->emin;
    const bool scale_up = tiny && (fpscr & FPSCR_UE) != 0;
    i32 q = (tiny && !scale_up) ? p->emin - (i32)(p->bits - 1) : e - (i32)(p->bits - 1);
    const i32 shift = q - u.exp;

    u128 mant;
    int frac;
    if (shift <= 0) {
        mant = u.sig << (u32)(-shift);
        frac = FRAC_ZERO;
    } else if (shift >= 128) {
        mant = 0;
        frac = FRAC_BELOW_HALF;
    } else {
        const u128 rem = u.sig & (((u128)1 << shift) - 1);
        const u128 half = (u128)1 << (shift - 1);
        mant = u.sig >> shift;
        if (rem == 0)
            frac = FRAC_ZERO;
        else if (rem < half)
            frac = FRAC_BELOW_HALF;
        else if (rem == half)
            frac = FRAC_HALF;
        else
            frac = FRAC_ABOVE_HALF;
    }

    bool inc;
    switch (rn) {
    case 0:
        inc = frac == FRAC_ABOVE_HALF || (frac == FRAC_HALF && (mant & 1u) != 0);
        break;
    case 1:
        inc = false;
        break;
    case 2:
        inc = frac != FRAC_ZERO && !u.sign;
        break;
    default:
        inc = frac != FRAC_ZERO && u.sign;
        break;
    }
    if (inc)
        mant++;

    u32 exc = 0;
    u32 frfi = 0;
    if (frac != FRAC_ZERO) {
        exc |= FPSCR_XX;
        frfi |= FPSCR_FI;
    }
    if (inc)
        frfi |= FPSCR_FR;
    if (tiny && (scale_up || frac != FRAC_ZERO))
        exc |= FPSCR_UX;

    if (mant != 0 && (i32)_msb128(mant) + q > p->emax) {
        exc |= FPSCR_OX;
        if (fpscr & FPSCR_OE) {
            q -= p->bias_adjust;
        } else {
            const bool to_inf = rn == 0 || (rn == 2 && !u.sign) || (rn == 3 && u.sign);
            r->exc |= exc | FPSCR_XX;
            r->frfi = FPSCR_FI | (to_inf ? FPSCR_FR : 0u);
            return sign | (to_inf ? F64_EXP : p->max_finite);
        }
    }

    if (scale_up)
        q += p->bias_adjust;

    u64 result = sign | _bits(ldexp((double)(u64)mant, q));
    if ((fpscr & FPSCR_NI) != 0 && !scale_up && _is_denormal(result, prec)) {
        exc |= FPSCR_UX | FPSCR_XX;
        frfi = FPSCR_FI;
        result = sign;
    }

    r->exc |= exc;
    r->frfi = frfi;
    return result;
}

static u64 _round_sum(Unpacked u, int prec, u32 fpscr, LaneResult *r)
{
    if (u.sig == 0)
        return _cancel_zero(fpscr);
    return _round(u, prec, fpscr, r);
}

static u64 _invalid(LaneResult *r, u32 exc)
{
    r->exc |= exc;
    r->invalid = true;
    return F64_DEFAULT_QNAN;
}

static u64 _sum(u64 x, u64 y, int prec, u32 fpscr, LaneResult *r)
{
    if (_is_inf(x) && _is_inf(y) && _is_neg(x) != _is_neg(y))
        return _invalid(r, FPSCR_VXISI);
    if (_is_inf(x))
        return x;
    if (_is_inf(y))
        return y;
    if (_is_zero(x) && _is_zero(y))
        return _is_neg(x) == _is_neg(y) ? x : _cancel_zero(fpscr);
    if (_is_zero(x))
        return _round(_unpack(y), prec, fpscr, r);
    if (_is_zero(y))
        return _round(_unpack(x), prec, fpscr, r);
    return _round_sum(_add_exact(_unpack(x), _unpack(y)), prec, fpscr, r);
}

static u64 _product(u64 x, u64 y, int prec, u32 fpscr, LaneResult *r)
{
    const u64 sign = (x ^ y) & F64_SIGN;
    if ((_is_inf(x) && _is_zero(y)) || (_is_zero(x) && _is_inf(y)))
        return _invalid(r, FPSCR_VXIMZ);
    if (_is_inf(x) || _is_inf(y))
        return sign | F64_EXP;
    if (_is_zero(x) || _is_zero(y))
        return sign;
    return _round(_mul_exact(_unpack(x), _unpack(y)), prec, fpscr, r);
}

static u64 _quotient(u64 x, u64 y, int prec, u32 fpscr, LaneResult *r)
{
    const u64 sign = (x ^ y) & F64_SIGN;
    if (_is_inf(x) && _is_inf(y))
        return _invalid(r, FPSCR_VXIDI);
    if (_is_zero(x) && _is_zero(y))
        return _invalid(r, FPSCR_VXZDZ);
    if (_is_inf(x))
        return sign | F64_EXP;
    if (_is_inf(y))
        return sign;
    if (_is_zero(y)) {
        r->exc |= FPSCR_ZX;
        r->zero_divide = true;
        return sign | F64_EXP;
    }
    if (_is_zero(x))
        return sign;
    return _round(_div_exact(_unpack(x), _unpack(y)), prec, fpscr, r);
}

static u64 _fused(u64 a, u64 c, u64 y, int prec, u32 fpscr, LaneResult *r)
{
    const bool pneg = _is_neg(a) != _is_neg(c);
    const bool pinf = _is_inf(a) || _is_inf(c);
    const bool pzero = _is_zero(a) || _is_zero(c);

    if ((_is_inf(a) && _is_zero(c)) || (_is_zero(a) && _is_inf(c)))
        return _invalid(r, FPSCR_VXIMZ);
    if (pinf && _is_inf(y) && pneg != _is_neg(y))
        return _invalid(r, FPSCR_VXISI);
    if (pinf)
        return (pneg ? F64_SIGN : 0) | F64_EXP;
    if (_is_inf(y))
        return y;
    if (pzero && _is_zero(y))
        return pneg == _is_neg(y) ? y : _cancel_zero(fpscr);
    if (pzero)
        return _round(_unpack(y), prec, fpscr, r);

    const Unpacked product = _mul_exact(_unpack(a), _unpack(c));
    if (_is_zero(y))
        return _round(product, prec, fpscr, r);
    return _round_sum(_add_exact(product, _unpack(y)), prec, fpscr, r);
}

static u64 _rsqrte(u64 b, int prec, u32 fpscr, LaneResult *r)
{
    if (_is_zero(b)) {
        r->exc |= FPSCR_ZX;
        r->zero_divide = true;
        return (b & F64_SIGN) | F64_EXP;
    }
    if (_is_neg(b))
        return _invalid(r, FPSCR_VXSQRT);
    if (_is_inf(b))
        return 0;

    const u64 estimate = _bits(1.0 / sqrt(_value(b)));
    if (prec == PREC_DOUBLE)
        return estimate;

    LaneResult ignored = {0};
    return _round(_unpack(estimate), PREC_SINGLE, fpscr, &ignored);
}

static LaneResult _arith(int op, u64 a, u64 b, u64 c, u32 fpscr, int prec)
{
    LaneResult r = {.value = F64_DEFAULT_QNAN};

    u64 operands[3] = {a, b, c};
    u32 count = 3;
    switch (op) {
    case ARITH_ADD:
    case ARITH_SUB:
    case ARITH_DIV:
        count = 2;
        break;
    case ARITH_MUL:
        operands[1] = c;
        count = 2;
        break;
    case ARITH_RES:
    case ARITH_RSQRTE:
    case ARITH_RSP:
        operands[0] = b;
        count = 1;
        break;
    default:
        break;
    }

    for (u32 i = 0; i < count; i++) {
        if (_is_snan(operands[i])) {
            r.exc |= FPSCR_VXSNAN;
            r.invalid = true;
        }
    }
    for (u32 i = 0; i < count; i++) {
        if (_is_nan(operands[i])) {
            r.value = _quiet(operands[i], prec);
            return r;
        }
    }

    const int operand_prec = op == ARITH_RSP ? PREC_DOUBLE : prec;
    a = _flush(a, operand_prec, fpscr);
    b = _flush(b, operand_prec, fpscr);
    c = _flush(c, operand_prec, fpscr);

    switch (op) {
    case ARITH_ADD:
        r.value = _sum(a, b, prec, fpscr, &r);
        break;
    case ARITH_SUB:
        r.value = _sum(a, b ^ F64_SIGN, prec, fpscr, &r);
        break;
    case ARITH_MUL:
        r.value = _product(a, c, prec, fpscr, &r);
        break;
    case ARITH_DIV:
        r.value = _quotient(a, b, prec, fpscr, &r);
        break;
    case ARITH_MADD:
    case ARITH_NMADD:
        r.value = _fused(a, c, b, prec, fpscr, &r);
        break;
    case ARITH_MSUB:
    case ARITH_NMSUB:
        r.value = _fused(a, c, b ^ F64_SIGN, prec, fpscr, &r);
        break;
    case ARITH_RES:
        r.value = _quotient(F64_ONE, b, prec, fpscr, &r);
        r.exc &= ~FPSCR_XX;
        r.frfi = 0;
        break;
    case ARITH_RSQRTE:
        r.value = _rsqrte(b, prec, fpscr, &r);
        r.frfi = 0;
        break;
    default:
        if (_is_inf(b) || _is_zero(b))
            r.value = b;
        else
            r.value = _round(_unpack(b), PREC_SINGLE, fpscr, &r);
        break;
    }

    if (!r.invalid && (op == ARITH_NMADD || op == ARITH_NMSUB))
        r.value ^= F64_SIGN;

    return r;
}

static u32 _classify(u64 v, int prec)
{
    const bool neg = _is_neg(v);
    if (_is_nan(v))
        return FPRF_QNAN;
    if (_is_inf(v))
        return neg ? FPRF_NEG_INF : FPRF_POS_INF;
    if (_is_zero(v))
        return neg ? FPRF_NEG_ZERO : FPRF_POS_ZERO;
    if (_is_denormal(v, prec))
        return neg ? FPRF_NEG_DENORMAL : FPRF_POS_DENORMAL;
    return neg ? FPRF_NEG_NORMAL : FPRF_POS_NORMAL;
}

static u32 _summarize(u32 fpscr)
{
    fpscr &= ~(FPSCR_VX | FPSCR_FEX);
    if (fpscr & FPSCR_VX_ALL)
        fpscr |= FPSCR_VX;

    const bool enabled = ((fpscr & FPSCR_VX) && (fpscr & FPSCR_VE)) ||
                         ((fpscr & FPSCR_OX) && (fpscr & FPSCR_OE)) ||
                         ((fpscr & FPSCR_UX) && (fpscr & FPSCR_UE)) ||
                         ((fpscr & FPSCR_ZX) && (fpscr & FPSCR_ZE)) ||
                         ((fpscr & FPSCR_XX) && (fpscr & FPSCR_XE));
    if (enabled)
        fpscr |= FPSCR_FEX;
    return fpscr;
}

static u32 _raise(u32 old, u32 fpscr)
{
    if ((fpscr & ~old) & FPSCR_EXCEPTIONS)
        fpscr |= FPSCR_FX;
    return _summarize(fpscr);
}

static bool _finish(CPU *cpu, const LaneResult *lanes, u32 count, int fprf_lane, int prec)
{
    const u32 old = cpu->state.fpscr;
    u32 exc = 0;
    u32 frfi = 0;
    bool invalid = false;
    bool zero_divide = false;
    for (u32 i = 0; i < count; i++) {
        exc |= lanes[i].exc;
        frfi |= lanes[i].frfi;
        invalid = invalid || lanes[i].invalid;
        zero_divide = zero_divide || lanes[i].zero_divide;
    }

    const bool suppress =
        (invalid && (old & FPSCR_VE) != 0) || (zero_divide && (old & FPSCR_ZE) != 0);

    u32 fpscr = (old | exc) & ~(FPSCR_FR | FPSCR_FI);
    if (!suppress) {
        fpscr |= frfi;
        if (fprf_lane >= 0)
            fpscr = (fpscr & ~FPSCR_FPRF_MASK) |
                    (_classify(lanes[fprf_lane].value, prec) << FPSCR_FPRF_SHIFT);
    }

    cpu->state.fpscr = _raise(old, fpscr);
    return suppress;
}

static void _set_cr_field(CPU *cpu, u32 field, u32 value)
{
    const u32 shift = 28u - 4u * field;
    cpu->state.cr = (cpu->state.cr & ~(0xfu << shift)) | ((value & 0xfu) << shift);
}

static void _compare(CPU *cpu, u64 a, u64 b, u32 field, bool ordered, int prec)
{
    const u32 old = cpu->state.fpscr;
    a = _flush(a, prec, old);
    b = _flush(b, prec, old);

    const bool unordered = _is_nan(a) || _is_nan(b);
    u32 cc;
    if (unordered)
        cc = 0x1u;
    else if (_value(a) < _value(b))
        cc = 0x8u;
    else if (_value(a) > _value(b))
        cc = 0x4u;
    else
        cc = 0x2u;

    u32 exc = 0;
    if (_is_snan(a) || _is_snan(b)) {
        exc |= FPSCR_VXSNAN;
        if (ordered && (old & FPSCR_VE) == 0)
            exc |= FPSCR_VXVC;
    } else if (ordered && unordered) {
        exc |= FPSCR_VXVC;
    }

    cpu->state.fpscr = _raise(old, (old & ~FPSCR_FPCC_MASK) | (cc << FPSCR_FPRF_SHIFT) | exc);
    _set_cr_field(cpu, field, cc);
}

static LaneResult _to_int(u64 b, u32 fpscr, bool toward_zero)
{
    LaneResult r = {0};
    u32 word;
    b = _flush(b, PREC_DOUBLE, fpscr);

    if (_is_nan(b)) {
        r.exc = FPSCR_VXCVI | (_is_snan(b) ? FPSCR_VXSNAN : 0u);
        r.invalid = true;
        word = 0x80000000u;
    } else if (_is_inf(b)) {
        r.exc = FPSCR_VXCVI;
        r.invalid = true;
        word = _is_neg(b) ? 0x80000000u : 0x7fffffffu;
    } else if (_is_zero(b)) {
        word = 0;
    } else {
        const Unpacked u = _unpack(b);
        const u32 rn = toward_zero ? 1u : fpscr & FPSCR_RN_MASK;

        u128 mag;
        int frac = FRAC_ZERO;
        if (u.exp >= 32) {
            mag = (u128)1 << 64;
        } else if (u.exp >= 0) {
            mag = u.sig << (u32)u.exp;
        } else if (u.exp <= -128) {
            mag = 0;
            frac = FRAC_BELOW_HALF;
        } else {
            const u32 shift = (u32)(-u.exp);
            const u128 rem = u.sig & (((u128)1 << shift) - 1);
            const u128 half = (u128)1 << (shift - 1);
            mag = u.sig >> shift;
            if (rem == 0)
                frac = FRAC_ZERO;
            else if (rem < half)
                frac = FRAC_BELOW_HALF;
            else if (rem == half)
                frac = FRAC_HALF;
            else
                frac = FRAC_ABOVE_HALF;
        }

        bool inc;
        switch (rn) {
        case 0:
            inc = frac == FRAC_ABOVE_HALF || (frac == FRAC_HALF && (mag & 1u) != 0);
            break;
        case 1:
            inc = false;
            break;
        case 2:
            inc = frac != FRAC_ZERO && !u.sign;
            break;
        default:
            inc = frac != FRAC_ZERO && u.sign;
            break;
        }
        if (inc)
            mag++;

        if ((!u.sign && mag > 0x7fffffffu) || (u.sign && mag > 0x80000000u)) {
            r.exc = FPSCR_VXCVI;
            r.invalid = true;
            word = u.sign ? 0x80000000u : 0x7fffffffu;
        } else {
            word = u.sign ? (u32)(0u - (u32)mag) : (u32)mag;
            if (frac != FRAC_ZERO) {
                r.exc = FPSCR_XX;
                r.frfi = FPSCR_FI;
            }
            if (inc)
                r.frfi |= FPSCR_FR;
        }
    }

    if (r.invalid && (fpscr & FPSCR_NI) != 0)
        word = 0x80000000u;

    r.value = F64_INT_HIGH_WORD | word;
    return r;
}

static u64 _select(u64 a, u64 c, u64 b, int prec, u32 fpscr)
{
    a = _flush(a, prec, fpscr);
    return !_is_nan(a) && _value(a) >= 0.0 ? c : b;
}

static FpInsn _decode(u32 insn)
{
    const u32 primary = insn >> 26;
    const u32 xo5 = (insn >> 1) & 0x1fu;
    const u32 xo10 = (insn >> 1) & 0x3ffu;

    if (primary == 59) {
        switch (xo5) {
        case 18:
            return FP_FDIVS;
        case 20:
            return FP_FSUBS;
        case 21:
            return FP_FADDS;
        case 24:
            return FP_FRES;
        case 25:
            return FP_FMULS;
        case 28:
            return FP_FMSUBS;
        case 29:
            return FP_FMADDS;
        case 30:
            return FP_FNMSUBS;
        case 31:
            return FP_FNMADDS;
        default:
            return FP_UNKNOWN;
        }
    }

    if (primary == 63) {
        if (xo5 >= 16) {
            switch (xo5) {
            case 18:
                return FP_FDIV;
            case 20:
                return FP_FSUB;
            case 21:
                return FP_FADD;
            case 23:
                return FP_FSEL;
            case 25:
                return FP_FMUL;
            case 26:
                return FP_FRSQRTE;
            case 28:
                return FP_FMSUB;
            case 29:
                return FP_FMADD;
            case 30:
                return FP_FNMSUB;
            case 31:
                return FP_FNMADD;
            default:
                return FP_UNKNOWN;
            }
        }
        switch (xo10) {
        case 0:
            return FP_FCMPU;
        case 12:
            return FP_FRSP;
        case 14:
            return FP_FCTIW;
        case 15:
            return FP_FCTIWZ;
        case 32:
            return FP_FCMPO;
        case 38:
            return FP_MTFSB1;
        case 64:
            return FP_MCRFS;
        case 70:
            return FP_MTFSB0;
        case 134:
            return FP_MTFSFI;
        case 583:
            return FP_MFFS;
        case 711:
            return FP_MTFSF;
        default:
            return FP_UNKNOWN;
        }
    }

    if (primary == 4) {
        switch (xo5) {
        case 10:
            return FP_PS_SUM0;
        case 11:
            return FP_PS_SUM1;
        case 12:
            return FP_PS_MULS0;
        case 13:
            return FP_PS_MULS1;
        case 14:
            return FP_PS_MADDS0;
        case 15:
            return FP_PS_MADDS1;
        case 18:
            return FP_PS_DIV;
        case 20:
            return FP_PS_SUB;
        case 21:
            return FP_PS_ADD;
        case 23:
            return FP_PS_SEL;
        case 24:
            return FP_PS_RES;
        case 25:
            return FP_PS_MUL;
        case 26:
            return FP_PS_RSQRTE;
        case 28:
            return FP_PS_MSUB;
        case 29:
            return FP_PS_MADD;
        case 30:
            return FP_PS_NMSUB;
        case 31:
            return FP_PS_NMADD;
        default:
            break;
        }
        switch (xo10) {
        case 0:
            return FP_PS_CMPU0;
        case 32:
            return FP_PS_CMPO0;
        case 64:
            return FP_PS_CMPU1;
        case 96:
            return FP_PS_CMPO1;
        case 72:
            return FP_PS_MR;
        case 136:
            return FP_PS_NABS;
        case 264:
            return FP_PS_ABS;
        case 528:
            return FP_PS_MERGE00;
        case 560:
            return FP_PS_MERGE01;
        case 592:
            return FP_PS_MERGE10;
        case 624:
            return FP_PS_MERGE11;
        default:
            return FP_UNKNOWN;
        }
    }

    return FP_UNKNOWN;
}

const char *fpu_mnemonic(u32 insn)
{
    return MNEMONICS[_decode(insn)];
}

static void _scalar(CPU *cpu, int op, int prec, bool pse)
{
    u64 *s = cpu->fp_args;
    const LaneResult r =
        _arith(op, s[FPU_ARG_A], s[FPU_ARG_B], s[FPU_ARG_C], cpu->state.fpscr, prec);
    if (_finish(cpu, &r, 1, 0, prec))
        return;
    s[FPU_ARG_D] = r.value;
    if (prec == PREC_SINGLE && pse)
        s[FPU_ARG_D + 1] = r.value;
}

static void _paired(CPU *cpu, int op, u32 c0, u32 c1)
{
    u64 *s = cpu->fp_args;
    const u32 fpscr = cpu->state.fpscr;
    const LaneResult r[2] = {
        _arith(op, s[FPU_ARG_A], s[FPU_ARG_B], s[FPU_ARG_C + c0], fpscr, PREC_SINGLE),
        _arith(op, s[FPU_ARG_A + 1], s[FPU_ARG_B + 1], s[FPU_ARG_C + c1], fpscr, PREC_SINGLE),
    };
    if (_finish(cpu, r, 2, 0, PREC_SINGLE))
        return;
    s[FPU_ARG_D] = r[0].value;
    s[FPU_ARG_D + 1] = r[1].value;
}

static void _paired_sum(CPU *cpu, u32 lane)
{
    u64 *s = cpu->fp_args;
    const LaneResult r =
        _arith(ARITH_ADD, s[FPU_ARG_A], s[FPU_ARG_B + 1], 0, cpu->state.fpscr, PREC_SINGLE);
    if (_finish(cpu, &r, 1, 0, PREC_SINGLE))
        return;
    s[FPU_ARG_D + lane] = r.value;
    s[FPU_ARG_D + 1 - lane] = s[FPU_ARG_C + 1 - lane];
}

static void _convert(CPU *cpu, bool toward_zero)
{
    u64 *s = cpu->fp_args;
    const LaneResult r = _to_int(s[FPU_ARG_B], cpu->state.fpscr, toward_zero);
    if (_finish(cpu, &r, 1, -1, PREC_DOUBLE))
        return;
    s[FPU_ARG_D] = r.value;
}

static void _mcrfs(CPU *cpu, u32 insn)
{
    const u32 crf_d = (insn >> 23) & 7u;
    const u32 crf_s = (insn >> 18) & 7u;
    const u32 shift = 28u - 4u * crf_s;
    const u32 old = cpu->state.fpscr;

    _set_cr_field(cpu, crf_d, old >> shift);
    const u32 cleared = (0xfu << shift) & (FPSCR_FX | FPSCR_EXCEPTIONS);
    cpu->state.fpscr = _summarize(old & ~cleared);
}

static void _mtfsb(CPU *cpu, u32 insn, bool set)
{
    const u32 bit = 1u << (31u - ((insn >> 21) & 0x1fu));
    const u32 old = cpu->state.fpscr;
    if (bit == FPSCR_FEX || bit == FPSCR_VX)
        return;

    u32 fpscr = set ? old | bit : old & ~bit;
    if (set && (bit & FPSCR_EXCEPTIONS) != 0 && (old & bit) == 0)
        fpscr |= FPSCR_FX;
    cpu->state.fpscr = _summarize(fpscr);
}

static void _move_to_fields(CPU *cpu, u32 field_mask, u32 value)
{
    u32 mask = 0;
    for (u32 i = 0; i < 8u; i++) {
        if (field_mask & (0x80u >> i))
            mask |= 0xf0000000u >> (4u * i);
    }
    mask &= ~(FPSCR_FEX | FPSCR_VX);
    cpu->state.fpscr = _summarize((cpu->state.fpscr & ~mask) | (value & mask));
}

void fpu_execute(CPU *cpu, u32 insn, bool pse)
{
    u64 *s = cpu->fp_args;
    const u32 fpscr = cpu->state.fpscr;

    switch (_decode(insn)) {
    case FP_FADDS:
        _scalar(cpu, ARITH_ADD, PREC_SINGLE, pse);
        break;
    case FP_FSUBS:
        _scalar(cpu, ARITH_SUB, PREC_SINGLE, pse);
        break;
    case FP_FMULS:
        _scalar(cpu, ARITH_MUL, PREC_SINGLE, pse);
        break;
    case FP_FDIVS:
        _scalar(cpu, ARITH_DIV, PREC_SINGLE, pse);
        break;
    case FP_FRES:
        _scalar(cpu, ARITH_RES, PREC_SINGLE, pse);
        break;
    case FP_FMADDS:
        _scalar(cpu, ARITH_MADD, PREC_SINGLE, pse);
        break;
    case FP_FMSUBS:
        _scalar(cpu, ARITH_MSUB, PREC_SINGLE, pse);
        break;
    case FP_FNMADDS:
        _scalar(cpu, ARITH_NMADD, PREC_SINGLE, pse);
        break;
    case FP_FNMSUBS:
        _scalar(cpu, ARITH_NMSUB, PREC_SINGLE, pse);
        break;
    case FP_FRSP:
        _scalar(cpu, ARITH_RSP, PREC_SINGLE, pse);
        break;
    case FP_FADD:
        _scalar(cpu, ARITH_ADD, PREC_DOUBLE, pse);
        break;
    case FP_FSUB:
        _scalar(cpu, ARITH_SUB, PREC_DOUBLE, pse);
        break;
    case FP_FMUL:
        _scalar(cpu, ARITH_MUL, PREC_DOUBLE, pse);
        break;
    case FP_FDIV:
        _scalar(cpu, ARITH_DIV, PREC_DOUBLE, pse);
        break;
    case FP_FRSQRTE:
        _scalar(cpu, ARITH_RSQRTE, PREC_DOUBLE, pse);
        break;
    case FP_FMADD:
        _scalar(cpu, ARITH_MADD, PREC_DOUBLE, pse);
        break;
    case FP_FMSUB:
        _scalar(cpu, ARITH_MSUB, PREC_DOUBLE, pse);
        break;
    case FP_FNMADD:
        _scalar(cpu, ARITH_NMADD, PREC_DOUBLE, pse);
        break;
    case FP_FNMSUB:
        _scalar(cpu, ARITH_NMSUB, PREC_DOUBLE, pse);
        break;
    case FP_FSEL:
        s[FPU_ARG_D] = _select(s[FPU_ARG_A], s[FPU_ARG_C], s[FPU_ARG_B], PREC_DOUBLE, fpscr);
        break;
    case FP_FCMPU:
        _compare(cpu, s[FPU_ARG_A], s[FPU_ARG_B], (insn >> 23) & 7u, false, PREC_DOUBLE);
        break;
    case FP_FCMPO:
        _compare(cpu, s[FPU_ARG_A], s[FPU_ARG_B], (insn >> 23) & 7u, true, PREC_DOUBLE);
        break;
    case FP_FCTIW:
        _convert(cpu, false);
        break;
    case FP_FCTIWZ:
        _convert(cpu, true);
        break;
    case FP_MFFS:
        s[FPU_ARG_D] = F64_INT_HIGH_WORD | fpscr;
        break;
    case FP_MCRFS:
        _mcrfs(cpu, insn);
        break;
    case FP_MTFSB0:
        _mtfsb(cpu, insn, false);
        break;
    case FP_MTFSB1:
        _mtfsb(cpu, insn, true);
        break;
    case FP_MTFSFI: {
        const u32 field = (insn >> 23) & 7u;
        const u32 imm = (insn >> 12) & 0xfu;
        _move_to_fields(cpu, 0x80u >> field, imm << (28u - 4u * field));
        break;
    }
    case FP_MTFSF:
        _move_to_fields(cpu, (insn >> 17) & 0xffu, (u32)s[FPU_ARG_B]);
        break;
    case FP_PS_ADD:
        _paired(cpu, ARITH_ADD, 0, 1);
        break;
    case FP_PS_SUB:
        _paired(cpu, ARITH_SUB, 0, 1);
        break;
    case FP_PS_MUL:
        _paired(cpu, ARITH_MUL, 0, 1);
        break;
    case FP_PS_DIV:
        _paired(cpu, ARITH_DIV, 0, 1);
        break;
    case FP_PS_MADD:
        _paired(cpu, ARITH_MADD, 0, 1);
        break;
    case FP_PS_MSUB:
        _paired(cpu, ARITH_MSUB, 0, 1);
        break;
    case FP_PS_NMADD:
        _paired(cpu, ARITH_NMADD, 0, 1);
        break;
    case FP_PS_NMSUB:
        _paired(cpu, ARITH_NMSUB, 0, 1);
        break;
    case FP_PS_MULS0:
        _paired(cpu, ARITH_MUL, 0, 0);
        break;
    case FP_PS_MULS1:
        _paired(cpu, ARITH_MUL, 1, 1);
        break;
    case FP_PS_MADDS0:
        _paired(cpu, ARITH_MADD, 0, 0);
        break;
    case FP_PS_MADDS1:
        _paired(cpu, ARITH_MADD, 1, 1);
        break;
    case FP_PS_RES:
        _paired(cpu, ARITH_RES, 0, 1);
        break;
    case FP_PS_RSQRTE:
        _paired(cpu, ARITH_RSQRTE, 0, 1);
        break;
    case FP_PS_SUM0:
        _paired_sum(cpu, 0);
        break;
    case FP_PS_SUM1:
        _paired_sum(cpu, 1);
        break;
    case FP_PS_SEL:
        s[FPU_ARG_D] = _select(s[FPU_ARG_A], s[FPU_ARG_C], s[FPU_ARG_B], PREC_SINGLE, fpscr);
        s[FPU_ARG_D + 1] =
            _select(s[FPU_ARG_A + 1], s[FPU_ARG_C + 1], s[FPU_ARG_B + 1], PREC_SINGLE, fpscr);
        break;
    case FP_PS_MR:
        s[FPU_ARG_D] = s[FPU_ARG_B];
        s[FPU_ARG_D + 1] = s[FPU_ARG_B + 1];
        break;
    case FP_PS_ABS:
        s[FPU_ARG_D] = s[FPU_ARG_B] & ~F64_SIGN;
        s[FPU_ARG_D + 1] = s[FPU_ARG_B + 1] & ~F64_SIGN;
        break;
    case FP_PS_NABS:
        s[FPU_ARG_D] = s[FPU_ARG_B] | F64_SIGN;
        s[FPU_ARG_D + 1] = s[FPU_ARG_B + 1] | F64_SIGN;
        break;
    case FP_PS_MERGE00:
        s[FPU_ARG_D + 1] = s[FPU_ARG_B];
        s[FPU_ARG_D] = s[FPU_ARG_A];
        break;
    case FP_PS_MERGE01:
        s[FPU_ARG_D + 1] = s[FPU_ARG_B + 1];
        s[FPU_ARG_D] = s[FPU_ARG_A];
        break;
    case FP_PS_MERGE10: {
        const u64 ps0 = s[FPU_ARG_A + 1];
        s[FPU_ARG_D + 1] = s[FPU_ARG_B];
        s[FPU_ARG_D] = ps0;
        break;
    }
    case FP_PS_MERGE11: {
        const u64 ps0 = s[FPU_ARG_A + 1];
        s[FPU_ARG_D + 1] = s[FPU_ARG_B + 1];
        s[FPU_ARG_D] = ps0;
        break;
    }
    case FP_PS_CMPU0:
        _compare(cpu, s[FPU_ARG_A], s[FPU_ARG_B], (insn >> 23) & 7u, false, PREC_SINGLE);
        break;
    case FP_PS_CMPO0:
        _compare(cpu, s[FPU_ARG_A], s[FPU_ARG_B], (insn >> 23) & 7u, true, PREC_SINGLE);
        break;
    case FP_PS_CMPU1:
        _compare(cpu, s[FPU_ARG_A + 1], s[FPU_ARG_B + 1], (insn >> 23) & 7u, false, PREC_SINGLE);
        break;
    case FP_PS_CMPO1:
        _compare(cpu, s[FPU_ARG_A + 1], s[FPU_ARG_B + 1], (insn >> 23) & 7u, true, PREC_SINGLE);
        break;
    default:
        return;
    }

    if (insn & 1u)
        _set_cr_field(cpu, 1, cpu->state.fpscr >> 28);
}
