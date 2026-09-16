#pragma once

#include "cpu/cpu_types.h"
#include "disc/disc.h"

#include <stddef.h>
#define TB_INITIAL_CAPACITY 0x2000
#define TB_MAX_CAPACITY (0x1000 * 16)
#define TB_MAX_BYTES_PER_GUEST_INSTRUCTION 256
#define TB_EPILOGUE_MAX_BYTES 32
#define HOST_INSTRUCTION_RET 0xD65F03C0u
#define TB_TRACE(...) printf(__VA_ARGS__)
#define TB_TRACE_CODE(ptr, n_instrs) _print_code_block((ptr), (n_instrs))

#define SPR_XER 1
#define SPR_LR 8
#define SPR_TBL 268
#define SPR_TBU 269
#define SPR_TBL_WRITE 284
#define SPR_TBU_WRITE 285
#define SPR_HID2 920
#define SPR_TERMINATING_INDEXES(spr) (spr == SPR_LR || spr == SPR_HID2)

#define write_to_buffer(buffer, ...)                                                               \
    write_buffer_impl((buffer), (const struct block[]){__VA_ARGS__},                               \
                      sizeof((const struct block[]){__VA_ARGS__}) / sizeof(struct block))

typedef u32 FPRUsageBitmap;

typedef struct {
    const void *code;
    size_t size;
} TranslationBlockCore;

#define TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS 1

typedef struct {

    TranslationBlockCore core;

    u8 type;

    u32 pc_at_start;
    u32 msr_at_start;
    u32 hid2_at_start;

    // for linking tb´s together
    /*
      uint16_t jmp_offset[2];
      uint16_t jmp_insn_offset[2];
      uintptr_t jmp_target_addr[2];
    */
} TranslationBlock;

typedef struct {
    u8 *code;     /* base of the mapping                       */
    u32 size;     /* bytes written so far                      */
    u32 capacity; /* bytes currently mapped                    */
} CodeBuffer;

void init_translation(Disc *disc);

void deconstruct_translation(void);

TranslationBlock *tb_lookup(CPU *cpu, CpuMode cpu_mode);

bool tb_translate(CPU *cpu, CpuMode cpu_mode, TranslationBlock *out_tb);

int tb_finilize(TranslationBlock *tb);

void run_tb(TranslationBlock *block, CPU *cpu);
// code buffer

bool code_buffer_init(CodeBuffer *cb, u32 capacity);

bool code_buffer_make_executable(CodeBuffer *cb);

bool code_buffer_reserve(CodeBuffer *cb, u32 extra);

void code_buffer_destroy(CodeBuffer *cb);
