#include "cpu/cpu.h"
#include <pthread.h>
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "cpu/translation/translation.h"
#include "disc/disc.h"
#include <_abort.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_pthread/_pthread_t.h>
#include <sys/cdefs.h>
static CPU *static_cpu_ptr;

static void deconstruct_cpu(CPU *self)
{
    UNUSED(self);
    deconstruct_translation();
}

static CpuMode get_current_cpu_mode(CPU *self)
{
    // TODO: implement
    CpuMode m;
    m.val = self->state.msr;
    return m;
}

static void main_loop(CPU *self)
{
    TranslationBlock *tb;
    CpuMode m = self->get_current_cpu_mode(self);

    // TODO: implement interrupt check and isr
    // while (!waiting_interrupt(self)) {
    while (true) {
        // DEBUG_PRINT("cycle \n");

        tb = tb_lookup(self, m);
        if (tb == NULL_PTR) {
            // TODO:
            // code block is not present in hash table and has to be translated

            // NOTE: if I add thread make sure to use locks here

            TranslationBlock *new_tb = malloc(sizeof(TranslationBlock));

            assert(new_tb != NULL_PTR);

            if (!tb_translate(self, m, new_tb, false)) {
                fprintf(stderr, "translation failed at pc 0x%08x\n", self->state.pc);
                free(new_tb);
                assert(!"tb_translate failed: guest code block could not be translated");
            }

            if (tb_finilize(new_tb) != 0) {
                fprintf(stderr, "tb_finilize failed at pc 0x%08x\n", self->state.pc);
                free(new_tb);
                assert(!"tb_finilize failed: translated block could not be finalized");
            }

            tb = new_tb;
        }

        self->print_state(self);
        run_tb(tb, self);
        self->print_state(self);
    }
}

static void _boot(CPU *self)
{
    self->state.msr = 0;

    // self->state.msr = BIT_SET(self->state.msr, 25); // boot bit
    self->state.pc = self->bus->entry_point;
}
static void print_cpu_state(CPU *self)
{
    const CpuState *st = &self->state;
    const CpuRegisters *rg = &self->registers;
    const CpuSpecialPurposeRegisters *spr = &self->special_purpose_registers;

    DEBUG_PRINT("================== CPU ==================\n");
    DEBUG_PRINT("PC    = 0x%08X\n", st->pc);
    DEBUG_PRINT("CR    = 0x%08X\n", st->cr);
    DEBUG_PRINT("XER   = 0x%08X\n", st->xer);
    DEBUG_PRINT("FPSCR = 0x%08X\n", st->fpscr);
    DEBUG_PRINT("MSR   = 0x%08X\n", st->msr);

    DEBUG_PRINT("\n------------------ GPR ------------------\n");
    for (u32 i = 0; i < 32; i++)
        DEBUG_PRINT("r%-2u = 0x%08X%s", i, rg->gpio[i], (i % 4 == 3) ? "\n" : "  ");

    DEBUG_PRINT("\n------------------ SR -------------------\n");
    for (u32 i = 0; i < 16; i++)
        DEBUG_PRINT("sr%-2u = 0x%08X%s", i, rg->sr[i], (i % 4 == 3) ? "\n" : "  ");

    DEBUG_PRINT("\n------------------ SPR ------------------\n");
    DEBUG_PRINT("LR   = 0x%08X   (spr8)\n", spr->lr);
    DEBUG_PRINT("HID2 = 0x%08X   (spr920)\n", spr->hid2);
    DEBUG_PRINT("IABR = 0x%08X   (spr1010)\n", spr->iabr);
    DEBUG_PRINT("DABR = 0x%08X   (spr1013)\n", spr->dabr);
    DEBUG_PRINT("=========================================\n");
}

static void _helper_write_word_to_bus(u32 adr, u32 val)
{
    static_cpu_ptr->bus->write(static_cpu_ptr->bus, adr, val, 4);
}
static void _helper_write_byte_to_bus(u32 adr, u32 val)
{
    static_cpu_ptr->bus->write(static_cpu_ptr->bus, adr, val & 0xff, 1);
}
static void _helper_write_half_to_bus(u32 adr, u32 val)
{
    static_cpu_ptr->bus->write(static_cpu_ptr->bus, adr, val & 0xffff, 2);
}
static void _helper_write_double_word_to_bus(u32 adr, u64 val)
{
    static_cpu_ptr->bus->write(static_cpu_ptr->bus, adr, val, 8);
}
static u32 _helper_read_word_from_bus(u32 adr)
{
    u32 val = (u32)static_cpu_ptr->bus->read(static_cpu_ptr->bus, adr, 4);
    return val;
}
static u32 _helper_read_half_word_from_bus(u32 adr)
{
    u32 val = (u16)static_cpu_ptr->bus->read(static_cpu_ptr->bus, adr, 2);
    return val;
}
static u64 _helper_read_double_word_from_bus(u32 adr)
{
    u64 val = static_cpu_ptr->bus->read(static_cpu_ptr->bus, adr, 8);

    return val;
}
static u32 _helper_read_byte(u32 adr)
{
    u32 val = (u8)static_cpu_ptr->bus->read(static_cpu_ptr->bus, adr, 1);

    return val & 0xff;
}

typedef struct {
    u64 ps0;
    u64 ps1;
} PairedSingleValue;

static u64 _double_to_bits(double v)
{
    union {
        double d;
        u64 u;
    } c = {.d = v};
    return c.u;
}

static double _bits_to_double(u64 b)
{
    union {
        u64 u;
        double d;
    } c = {.u = b};
    return c.d;
}

static u64 _single_bits_to_double_bits(u32 w)
{
    const u64 sign = (u64)(w >> 31) << 63;
    const u32 exp = (w >> 23) & 0xffu;
    const u32 frac = w & 0x7fffffu;

    if (exp != 0u) {
        const u64 e = (exp == 0xffu) ? 0x7ffull : (u64)exp + 896ull;
        return sign | (e << 52) | ((u64)frac << 29);
    }

    if (frac == 0u)
        return sign;

    u32 k = 22u;
    while (((frac >> k) & 1u) == 0u)
        k--;

    return sign | ((u64)(k + 874u) << 52) | ((u64)(frac - (1u << k)) << (52u - k));
}

static u32 _double_bits_to_single_bits(u64 b)
{
    const u32 sign = (u32)(b >> 63);
    const u32 exp = (u32)((b >> 52) & 0x7ffu);
    const u64 frac = b & 0xfffffffffffffull;

    if (exp > 896u || (b & 0x7fffffffffffffffull) == 0ull) {
        const u32 exp8 = (((exp >> 10) & 1u) << 7) | (exp & 0x7fu);
        return (sign << 31) | (exp8 << 23) | (u32)(frac >> 29);
    }

    if (exp >= 874u) {
        const u64 m = frac | 0x10000000000000ull;
        return (sign << 31) | ((u32)(m >> (926u - exp)) & 0x7fffffu);
    }

    return sign << 31;
}

static i32 _quantize_scale(u32 scale_field)
{
    return (i32)((scale_field ^ 0x20u) - 0x20u);
}

static u32 _quantized_size(u32 type)
{
    if (type == 4u || type == 6u)
        return 1u;
    if (type == 5u || type == 7u)
        return 2u;
    return 4u;
}

static u64 _dequantize(u32 raw, u32 type, u32 scale_field)
{
    if (type < 4u)
        return _single_bits_to_double_bits(raw);

    i32 v;
    switch (type) {
    case 4:
        v = (i32)(u8)raw;
        break;
    case 5:
        v = (i32)(u16)raw;
        break;
    case 6:
        v = (i32)(i8)(u8)raw;
        break;
    default:
        v = (i32)(i16)(u16)raw;
        break;
    }

    const double factor = _bits_to_double((u64)(1023 - _quantize_scale(scale_field)) << 52);
    return _double_to_bits((double)v * factor);
}

static u32 _quantize(u64 bits, u32 type, u32 scale_field)
{
    if (type < 4u)
        return _double_bits_to_single_bits(bits);

    i32 lo;
    i32 hi;
    switch (type) {
    case 4:
        lo = 0;
        hi = 255;
        break;
    case 5:
        lo = 0;
        hi = 65535;
        break;
    case 6:
        lo = -128;
        hi = 127;
        break;
    default:
        lo = -32768;
        hi = 32767;
        break;
    }

    const double factor = _bits_to_double((u64)(1023 + _quantize_scale(scale_field)) << 52);
    const double v = _bits_to_double(bits) * factor;

    i32 r;
    if (v != v)
        r = hi;
    else if (v > (double)hi)
        r = hi;
    else if (v < (double)lo)
        r = lo;
    else
        r = (i32)v;

    return (u32)r;
}

static PairedSingleValue _helper_quantized_load(u32 adr, u32 gqr, u32 w)
{
    const u32 type = (gqr >> 16) & 7u;
    const u32 scale = (gqr >> 24) & 0x3fu;
    const u32 size = _quantized_size(type);

    PairedSingleValue out;
    out.ps0 =
        _dequantize((u32)static_cpu_ptr->bus->read(static_cpu_ptr->bus, adr, size), type, scale);

    if (w != 0u)
        out.ps1 = 0x3ff0000000000000ull;
    else
        out.ps1 = _dequantize((u32)static_cpu_ptr->bus->read(static_cpu_ptr->bus, adr + size, size),
                              type, scale);

    return out;
}

static void _helper_quantized_store(u32 adr, u32 gqr, u32 w, u64 ps0, u64 ps1)
{
    const u32 type = gqr & 7u;
    const u32 scale = (gqr >> 8) & 0x3fu;
    const u32 size = _quantized_size(type);

    static_cpu_ptr->bus->write(static_cpu_ptr->bus, adr, _quantize(ps0, type, scale), size);

    if (w == 0u)
        static_cpu_ptr->bus->write(static_cpu_ptr->bus, adr + size, _quantize(ps1, type, scale),
                                   size);
}

static void _helper_write_switch_to_exception(u32 cia)
{
    move_from_to_scratch_regs(&static_cpu_ptr->fpu, &static_cpu_ptr->fpu_scratch);

    static_cpu_ptr->special_purpose_registers.buf[26] = cia + 4;
    static_cpu_ptr->special_purpose_registers.buf[27] = static_cpu_ptr->state.msr & 0xCEFF03E1;

    if ((static_cpu_ptr->state.msr >> 25) & 1) {
        static_cpu_ptr->state.pc = 0xFFF00C00;
    } else {

        static_cpu_ptr->state.pc = 0x00000C00;
    }

    static_cpu_ptr->state.msr = ((static_cpu_ptr->state.msr << 16) & 1) |
                                (static_cpu_ptr->state.msr & ((1 << 25) | (1 << 19)));

    return;
}

static u8 _fpu_get_sep_bit(CPU *self)
{
    return (self->special_purpose_registers.hid2 >> 31 - 2) & 1;
}

static const FPU FPU_TEMPLATE = {
    .get_pse_bit = _fpu_get_sep_bit,
};

static void start(CPU *self)
{
    pthread_t main_thread;
    pthread_t background_thread;

    // NOTE: pthread_create writes the new handle through the first argument, so it must be the
    // address of the pthread_t, not its (uninitialized) value
    if (pthread_create(&main_thread, NULL, (void *)self->main, self) != 0) {
        assert(!"pthread_create failed for main_thread");
    }
    if (pthread_create(&background_thread, NULL, (void *)self->background, self) != 0) {
        assert(!"pthread_create failed for background_thread");
    }

    pthread_join(main_thread, NULL);
    pthread_join(background_thread, NULL);
}

static const CPU CPU_TEMPLATE = {
    .registers = {0},
    .state = {0},
    .free = &deconstruct_cpu,
    .main = &main_loop,
    .start = &start,
    .background = &run_background,
    .get_current_cpu_mode = &get_current_cpu_mode,
    .boot = &_boot,
    .print_state = &print_cpu_state,
    .fpu = FPU_TEMPLATE,

    // NOTE: no libc functions !!!
    // NOTE if usage of lib functions in debug push and pop float regs
    //------------------------------------------------------------------------------------------
    .helper_functions[0] = (u64)&_helper_write_word_to_bus,
    .helper_functions[1] = (u64)&_helper_read_word_from_bus,
    .helper_functions[2] = (u64)&_helper_read_double_word_from_bus,
    .helper_functions[3] = (u64)&_helper_read_byte,
    .helper_functions[4] = (u64)&_helper_write_byte_to_bus,
    .helper_functions[5] = (u64)&_helper_write_half_to_bus,
    .helper_functions[6] = (u64)&_helper_read_half_word_from_bus,
    .helper_functions[7] = (u64)&_helper_write_double_word_to_bus,
    .helper_functions[8] = (u64)&_helper_write_switch_to_exception,
    .helper_functions[9] = (u64)&_helper_quantized_load,
    .helper_functions[10] = (u64)&_helper_quantized_store,
};

void init_cpu(CPU *self, Disc *disc, Bus *bus)
{
    *self = CPU_TEMPLATE;
    self->bus = bus;
    init_translation(disc);
    static_cpu_ptr = self;
}
