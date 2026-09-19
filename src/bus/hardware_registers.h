#pragma once

#include "core/config/config.h"
#include "cpu/cpu_types.h"

#define CP_CONTROL_REGISTER 0xCC000002

typedef struct GXFifoRegs {
    /* +0x00 */
    volatile uint16_t SR; // r    FIFO-Status

    /* +0x02 */
    volatile uint16_t CR; // r/w  FIFO-Steuerung

    /* +0x04 */
    volatile uint16_t Clear; // w    Interrupts/Metriken löschen

    /* +0x06 */
    volatile uint16_t PERF_SELECT; // r/w  Performance-Counter-Auswahl, mask 0x7

    /* +0x08 */
    uint16_t _pad08;

    /* +0x0A */
    volatile uint16_t UNK_0A; // r/w  unbekannt, mask 0x00FF

    /* +0x0C */
    uint16_t _pad0C;

    /* +0x0E */
    volatile uint16_t Token; // r/w  Token-Register (YAGCD)

    /* +0x10 */
    volatile uint16_t BBox[4]; // r/w  Bounding Box: l/r/t/b
                               //      +0x10 .. +0x16

    /* +0x18 .. +0x1E */
    uint16_t _pad18[4];

    /* +0x20 */
    volatile uint16_t FIFO_BASE_lo;
    /* +0x22 */
    volatile uint16_t FIFO_BASE_hi;

    /* +0x24 */
    volatile uint16_t FIFO_END_lo;
    /* +0x26 */
    volatile uint16_t FIFO_END_hi;

    /* +0x28 */
    volatile uint16_t HI_WATERMARK_lo;
    /* +0x2A */
    volatile uint16_t HI_WATERMARK_hi;

    /* +0x2C */
    volatile uint16_t LO_WATERMARK_lo;
    /* +0x2E */
    volatile uint16_t LO_WATERMARK_hi;

    /* +0x30 */
    volatile uint16_t RW_DISTANCE_lo;
    /* +0x32 */
    volatile uint16_t RW_DISTANCE_hi;

    /* +0x34 */
    volatile uint16_t WRITE_POINTER_lo;
    /* +0x36 */
    volatile uint16_t WRITE_POINTER_hi;

    /* +0x38 */
    volatile uint16_t READ_POINTER_lo;
    /* +0x3A */
    volatile uint16_t READ_POINTER_hi;

    /* +0x3C */
    volatile uint16_t BP_lo;
    /* +0x3E */
    volatile uint16_t BP_hi;

    /* +0x40 */
    volatile uint16_t XF_RASBUSY;

    /* +0x42 */
    volatile uint16_t XF_CLKS;

    /* +0x44 */
    volatile uint16_t XF_WAIT_IN;

    /* +0x46 */
    volatile uint16_t XF_WAIT_OUT;

    /* +0x48 .. +0x4E */
    uint16_t _pad48[4];

    /* +0x50 */
    volatile uint16_t VCACHE_METRIC_CHECK;

    /* +0x52 */
    volatile uint16_t VCACHE_METRIC_MISS;

    /* +0x54 */
    volatile uint16_t VCACHE_METRIC_STALL;

    /* +0x56 .. +0x5E */
    uint16_t _pad56[4];

    /* +0x60 */
    volatile uint16_t CLKS_PER_VTX_IN;

    /* +0x62 */
    volatile uint16_t CLKS_PER_VTX_OUT;

    /* +0x64 */
} PACKED GXFifoRegs;

void cp_write(CPU *cpu, u32 adr, u32 val, u32 size);
u64 cp_read(CPU *cpu, u32 adr, u32 size);
