#include "cpu/translation/translation.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "disc/disc.h"
#include <_abort.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <khash/khash.h>

KHASH_MAP_INIT_INT64(tb_map, TranslationBlock *)

static khash_t(tb_map) * t;

#define TB_CACHE_ENTRIES 10

static TranslationBlock *translation_block_cache[TB_CACHE_ENTRIES];
static TranslationBlock *translation_block_cache_scratch[TB_CACHE_ENTRIES];

void init_translation(Disc *disc)
{
    t = kh_init(tb_map);
}

void deconstruct_translation(void)
{
    kh_destroy(tb_map, t);
}

static inline u64 get_hash_from_state(const u32 pc, const u32 msr)
{
    return (u64)pc << 32 | msr;
}

void _empty(u64 pc)
{

    printf("run tb block for pc : 0x%016llx\n", pc);
}

TranslationBlock *tb_lookup(CPU *cpu, CpuMode cpu_mode)
{
    for (int i = 0; i < TB_CACHE_ENTRIES; i++) {
        if (translation_block_cache[i] == NULL_PTR)
            continue;
        if (translation_block_cache[i]->pc_at_start == cpu->state.pc &&
            translation_block_cache[i]->msr_at_start == cpu->state.msr) {
            return translation_block_cache[i];
        }
    }
    const khiter_t k = kh_get(tb_map, t, get_hash_from_state(cpu->state.pc, cpu->state.msr));
    return k == kh_end(t) ? NULL : kh_value(t, k);
}

int tb_finilize(TranslationBlock *tb)
{
    if (tb->core.code == NULL || tb->core.size == 0) {
        fprintf(stderr, "tb_finilize: leerer block fuer pc 0x%08x\n", tb->pc_at_start);
        return -1;
    }

    if (mprotect((void *)tb->core.code, tb->core.size, PROT_READ | PROT_EXEC) != 0) {
        perror("mprotect");
        return -1;
    }
    __builtin___clear_cache((char *)tb->core.code, (char *)tb->core.code + tb->core.size);

    tb->hash = get_hash_from_state(tb->pc_at_start, tb->msr_at_start);

    DEBUG_PRINT("added tb [0x%08x], with code adr : %p\n", tb->pc_at_start, tb->core.code);
    int ret;
    const khiter_t k = kh_put(tb_map, t, tb->hash, &ret);
    if (ret < 0) {
        assert(!"khash error ");
    }
    kh_value(t, k) = tb;
    return 0;
}

_Static_assert(offsetof(FPU, fpr) == 8, "FPU_FPR_OFFSET in run_tb_fpu.s");
_Static_assert(offsetof(FPU, ps1) == 264, "FPU_PS1_OFFSET in run_tb_fpu.s");

void run_tb(TranslationBlock *block, CPU *cpu)
{
    // printf("[RUN TB] now running : 0x%08x, with adr : %p\n", block->pc_at_start,
    // block->core.code);

    assert(block->core.code != NULL);

    if (block->type & TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS) {
        run_tb_with_fpu(block->core.code, &cpu->fpu);
        return;
    }

    void (*code)(void);
    *(uintptr_t *)&code = (uintptr_t)block->core.code;
    code();

    // shift out the least used one in the cache
    memcpy(&translation_block_cache_scratch[1], &translation_block_cache[0],
           (TB_CACHE_ENTRIES - 1) * sizeof(void *));

    translation_block_cache[0] = block;

    memcpy(&translation_block_cache[1], &translation_block_cache_scratch[1],
           (TB_CACHE_ENTRIES - 1) * sizeof(void *));
}
