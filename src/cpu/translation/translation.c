#include "cpu/translation/translation.h"
#include "core/config/config.h"
#include "disc/disc.h"
#include <assert.h>
#include <stdio.h>
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

TranslationBlock *tb_lookup(CPU *cpu, CpuMode cpu_mode)
{
    // TODO:
    // implement fast tb cache

    char buffer[26] = {0};

    assert(sprintf(buffer, "%016llx-%08x", cpu->state.pc, cpu_mode.val) != 26);

    TranslationBlock *tb = zhash_get(t, buffer);

    if (tb == NULL) {
        return tb;
    }

    // run the block
}
