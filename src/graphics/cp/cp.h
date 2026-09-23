#pragma once

#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include <sys/_pthread/_pthread_cond_t.h>
#include <sys/_pthread/_pthread_mutex_t.h>

#define CP_CONTROL_REGISTER 0xCC000002

#define OPCODE_NOP 0x00
#define OPCODE_INVL_VC 0x48
#define OPCODE_NOP_LENGTH 0x01

#define OPCODE_LOAD_BP_REG 0x61
#define OPCODE_LOAD_BP_REG_LENGTH 5

#define OPCODE_LOAD_CP_REG 0x08
#define OPCODE_LOAD_CP_REG_LENGTH 6

#define OPCODE_LOAD_XF_REG 0x10

#define OPCODE_CALL_DL 0x40
#define OPCODE_CALL_DL_LENGTH 9

#define OPCODE_PRIMITIVE_START 0x80
#define OPCODE_PRIMITIVE_END 0xBF

#define BP_SET_DRAW_DONE 0x45
#define BP_SET_PE_TOKEN 0x47

typedef struct {
    _Atomic u32 RW_DISTANCE;
    _Atomic u32 FIFO_BASE;
    _Atomic u32 FIFO_END;
    _Atomic u32 WRITE_POINTER;
    _Atomic u32 READ_POINTER;

} GXFifoFifoMarkers;

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
    volatile uint32_t _FIFO_BASE;

    /* +0x24 */
    volatile uint32_t _FIFO_END;

    /* +0x28 */
    volatile uint32_t HI_WATERMARK;

    /* +0x2C */
    volatile uint32_t LO_WATERMARK;

    /* +0x30 */
    volatile uint32_t _RW_DISTANCE;

    /* +0x34 */
    volatile uint32_t _WRITE_POINTER;

    /* +0x38 */
    volatile uint32_t _READ_POINTER;

    /* +0x3C */
    volatile uint32_t BP; /* +0x40 */
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

    GXFifoFifoMarkers fifo_markers;

} PACKED GXFifoRegs;

typedef struct {
    pthread_cond_t fifo_cond;
    pthread_mutex_t fifo_mutex;
} FifoControlRegisters;

void cp_write(CPU *cpu, u32 adr, u32 val, u32 size);
u64 cp_read(CPU *cpu, u32 adr, u32 size);
GXFifoRegs *get_cp_regs(void);
void cp_recieve_gather_pipe(CPU *cpu);
void cp_check_state(CPU *cpu);

u32 cp_get_cr_reg(void);
u32 cp_get_sr_reg(void);

void decode_data_stream(CPU *cpu, GXFifoRegs *cp_regs);
void execute_command(CPU *cpu, GXFifoRegs *command_processor_registers, const u8 op,
                     u8 **stream_ptr, bool decrease_rw_distance);

u64 read_stream(CPU *cpu, GXFifoRegs *command_processor_registers, u8 **stream, u8 size);
u32 get_size_of_vertex(u32 VCD_HI, u32 VCD_LO, u32 VAT_A, u32 VAT_B, u32 VAT_C);

void cp_init(CPU *cpu);
void cp_thread(CPU *cpu);

u32 *get_xf_register_pointer(void);
u32 *get_bp_register_pointer(void);
u32 *get_cp_register_pointer(void);
