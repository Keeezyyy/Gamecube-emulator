#include "cpu/translation/translation.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "disc/disc.h"
#include <_abort.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <zhash/zhash.h>

static struct ZHashTable *t;

/*
// create hash table
struct ZHashTable *zcreate_hash_table(void);

// free hash table (note that this only frees the table and the entry structs)
void zfree_hash_table(struct ZHashTable *hash_table);

// set key to val (if there is already a value, overwrite it)
void zhash_set(struct ZHashTable *hash_table, char *key, void *val);

// get the value stored at key (if no value, return NULL)
void *zhash_get(struct ZHashTable *hash_table, char *key);

// delete entry stored at key and return the value (if no value, return NULL)
void *zhash_delete(struct ZHashTable *hash_table, char *key);

// return true if there is a value stored at the key and false otherwise
bool zhash_exists(struct ZHashTable *hash_table, char *key);
*/

void init_translation(Disc *disc)
{
    t = zcreate_hash_table();
}

void deconstruct_translation(void)
{
    zfree_hash_table(t);
}

#define HASH_BUFFER_SIZE 128

static inline void get_hash_from_state(const u32 pc, const u32 msr, const u32 hid2, char *buffer)
{
    /* Nicht in ein assert() packen: im Release-Build (NDEBUG) faellt der
     * Rumpf sonst weg und jeder Zustand bekaeme denselben leeren Schluessel. */
    const int written = snprintf(buffer, HASH_BUFFER_SIZE, "%08x-%08x-%08x", pc, msr, hid2);
    assert(written > 0 && written < HASH_BUFFER_SIZE);
    (void)written;
}

void _empty(u32 pc)
{

    DEBUG_PRINT("run tb block for pc : 0x%08x\n", pc);
}

TranslationBlock *tb_lookup(CPU *cpu, CpuMode cpu_mode)
{
    // TODO:
    // implement fast tb cache

    char buffer[HASH_BUFFER_SIZE] = {0};

    get_hash_from_state(cpu->state.pc, cpu->state.msr, cpu->special_purpose_registers.hid2, buffer);

    return zhash_get(t, buffer);
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

    char buffer[HASH_BUFFER_SIZE] = {0};

    get_hash_from_state(tb->pc_at_start, tb->msr_at_start, tb->hid2_at_start, buffer);

    DEBUG_PRINT("added tb [0x%08x], with code adr : %p\n", tb->pc_at_start, tb->core.code);
    zhash_set(t, buffer, tb);
    return 0;
}

_Static_assert(offsetof(FPU, fpr) == 8, "FPU_FPR_OFFSET in run_tb_fpu.s");
_Static_assert(offsetof(FPU, ps1) == 264, "FPU_PS1_OFFSET in run_tb_fpu.s");

void run_tb(TranslationBlock *block, CPU *cpu)
{
    ////printf("[RUN TB] now running : 0x%08x, with adr : %p\n", block->pc_at_start,
    ///block->core.code);

    assert(block->core.code != NULL);

    if (block->type & TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS) {
        run_tb_with_fpu(block->core.code, &cpu->fpu);
        return;
    }

    void (*code)(void);
    *(uintptr_t *)&code = (uintptr_t)block->core.code;
    code();
}
