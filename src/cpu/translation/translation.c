#include "cpu/translation/translation.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"
#include "disc/disc.h"
#include <_abort.h>
#include <assert.h>
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

static inline void push_fp_regs(void)
{
    __asm__ volatile("sub sp, sp, #128\n"
                     "str q8,  [sp, #0]\n"
                     "str q9,  [sp, #16]\n"
                     "str q10, [sp, #32]\n"
                     "str q11, [sp, #48]\n"
                     "str q12, [sp, #64]\n"
                     "str q13, [sp, #80]\n"
                     "str q14, [sp, #96]\n"
                     "str q15, [sp, #112]\n" ::
                         : "memory");
}
static inline void pop_fp_regs(void)
{
    __asm__ volatile("ldr q8,  [sp, #0]\n"
                     "ldr q9,  [sp, #16]\n"
                     "ldr q10, [sp, #32]\n"
                     "ldr q11, [sp, #48]\n"
                     "ldr q12, [sp, #64]\n"
                     "ldr q13, [sp, #80]\n"
                     "ldr q14, [sp, #96]\n"
                     "ldr q15, [sp, #112]\n"
                     "add sp, sp, #128\n" ::
                         : "memory");
}
#define LOAD_FPR(fpu, n)                                                                           \
    __asm__ volatile("ldr d" #n ", [%0]\n"                                                         \
                     "ld1 {v" #n ".d}[1], [%1]"                                                    \
                     :                                                                             \
                     : "r"(&(fpu)->fpr[n]), "r"(&(fpu)->ps1[n])                                    \
                     : "memory")

#define STORE_FPR(fpu, n)                                                                          \
    __asm__ volatile("str d" #n ", [%0]\n"                                                         \
                     "st1 {v" #n ".d}[1], [%1]"                                                    \
                     :                                                                             \
                     : "r"(&(fpu)->fpr[n]), "r"(&(fpu)->ps1[n])                                    \
                     : "memory")

static inline void load_fp_regs(FPU *fpu)
{
    LOAD_FPR(fpu, 0);
    LOAD_FPR(fpu, 1);
    LOAD_FPR(fpu, 2);
    LOAD_FPR(fpu, 3);
    LOAD_FPR(fpu, 4);
    LOAD_FPR(fpu, 5);
    LOAD_FPR(fpu, 6);
    LOAD_FPR(fpu, 7);
    LOAD_FPR(fpu, 8);
    LOAD_FPR(fpu, 9);
    LOAD_FPR(fpu, 10);
    LOAD_FPR(fpu, 11);
    LOAD_FPR(fpu, 12);
    LOAD_FPR(fpu, 13);
    LOAD_FPR(fpu, 14);
    LOAD_FPR(fpu, 15);
    LOAD_FPR(fpu, 16);
    LOAD_FPR(fpu, 17);
    LOAD_FPR(fpu, 18);
    LOAD_FPR(fpu, 19);
    LOAD_FPR(fpu, 20);
    LOAD_FPR(fpu, 21);
    LOAD_FPR(fpu, 22);
    LOAD_FPR(fpu, 23);
    LOAD_FPR(fpu, 24);
    LOAD_FPR(fpu, 25);
    LOAD_FPR(fpu, 26);
    LOAD_FPR(fpu, 27);
    LOAD_FPR(fpu, 28);
    LOAD_FPR(fpu, 29);
    LOAD_FPR(fpu, 30);
    LOAD_FPR(fpu, 31);
}

static inline void store_fp_regs(FPU *fpu)
{
    STORE_FPR(fpu, 0);
    STORE_FPR(fpu, 1);
    STORE_FPR(fpu, 2);
    STORE_FPR(fpu, 3);
    STORE_FPR(fpu, 4);
    STORE_FPR(fpu, 5);
    STORE_FPR(fpu, 6);
    STORE_FPR(fpu, 7);
    STORE_FPR(fpu, 8);
    STORE_FPR(fpu, 9);
    STORE_FPR(fpu, 10);
    STORE_FPR(fpu, 11);
    STORE_FPR(fpu, 12);
    STORE_FPR(fpu, 13);
    STORE_FPR(fpu, 14);
    STORE_FPR(fpu, 15);
    STORE_FPR(fpu, 16);
    STORE_FPR(fpu, 17);
    STORE_FPR(fpu, 18);
    STORE_FPR(fpu, 19);
    STORE_FPR(fpu, 20);
    STORE_FPR(fpu, 21);
    STORE_FPR(fpu, 22);
    STORE_FPR(fpu, 23);
    STORE_FPR(fpu, 24);
    STORE_FPR(fpu, 25);
    STORE_FPR(fpu, 26);
    STORE_FPR(fpu, 27);
    STORE_FPR(fpu, 28);
    STORE_FPR(fpu, 29);
    STORE_FPR(fpu, 30);
    STORE_FPR(fpu, 31);
}

void run_tb(TranslationBlock *block, CPU *cpu)
{
    DEBUG_PRINT("[RUN TB] now running : 0x%08x, with adr : %p\n", block->pc_at_start, block->core.code);

    assert(block->core.code != NULL);

    if (block->type & TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS) {
        push_fp_regs();
        load_fp_regs(&cpu->fpu);
    }

    void (*code)(void);
    *(uintptr_t *)&code = (uintptr_t)block->core.code;
    code();

    if (block->type & TRANSLATION_BLOCK_TYPE_FLOATING_POINT_OPERATIONS) {
        // direkt nach code(): kein Aufruf dazwischen, der v0-v7/v16-v31 zerstoeren koennte
        store_fp_regs(&cpu->fpu);
        pop_fp_regs();
    }
}
