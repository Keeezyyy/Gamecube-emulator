#include "cpu/translation/translation.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "disc/disc.h"
#include <_abort.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <khash/khash.h>

KHASH_MAP_INIT_INT64(tb_map, TranslationBlock *)

static khash_t(tb_map) * t;

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

#define DIRECT_MAP_CACHE_BIT_LENGTH 12
#define DIRECT_MAP_CACHE_SIZE (1 << DIRECT_MAP_CACHE_BIT_LENGTH)

static TranslationBlock *direct_cache[DIRECT_MAP_CACHE_SIZE];

TranslationBlock *tb_lookup(CPU *cpu, CpuMode cpu_mode)
{

    const u16 cache_idx = cpu->state.pc & (DIRECT_MAP_CACHE_SIZE - 1);

    TranslationBlock *cache_entry = direct_cache[cache_idx];

    if (cache_entry != NULL_PTR && cache_entry->pc_at_start == cpu->state.pc &&
        cache_entry->msr_at_start == cpu->state.msr) {
        // cache hit
        return cache_entry;
    }

    const khiter_t k = kh_get(tb_map, t, get_hash_from_state(cpu->state.pc, cpu->state.msr));

    if (k != kh_end(t))
        direct_cache[cache_idx] = kh_value(t, k);

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
static void free_tb(TranslationBlock *tb)
{
    mprotect(tb->core.code, tb->core.size, PROT_READ | PROT_WRITE);
    munmap(tb->core.code, tb->core.size);
    free(tb);
}

void invalidate_tb(u32 adr_start, u32 adr_end)
{
    for (khiter_t k = kh_begin(t); k != kh_end(t); k++) {
        if (!kh_exist(t, k))
            continue;

        TranslationBlock *tb = kh_value(t, k);
        if (tb->pc_at_start > adr_end || tb->pc_at_end < adr_start)
            continue;

        if ((adr_start <= tb->pc_at_start && adr_end >= tb->pc_at_start) ||
            (adr_start <= tb->pc_at_end && adr_end >= tb->pc_at_end)) {

            const u16 cache_idx = tb->pc_at_start & (DIRECT_MAP_CACHE_SIZE - 1);

            if (direct_cache[cache_idx] == tb)
                direct_cache[cache_idx] = NULL_PTR;

            free_tb(tb);
            kh_del(tb_map, t, k);
        }
    }
}

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

    const u16 cache_idx = block->pc_at_start & (DIRECT_MAP_CACHE_SIZE - 1);
    direct_cache[cache_idx] = block;
}
