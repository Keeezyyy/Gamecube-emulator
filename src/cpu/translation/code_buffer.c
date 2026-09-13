#include "translation.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

bool code_buffer_init(CodeBuffer *cb, u32 capacity)
{
    void *mapping =
        mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) {
        perror("mmap");
        return false;
    }

    cb->code = mapping;
    cb->size = 0;
    cb->capacity = capacity;
    return true;
}

void code_buffer_destroy(CodeBuffer *cb)
{
    if (cb->code != NULL) {
        munmap(cb->code, cb->capacity);
        cb->code = NULL;
    }
}

bool code_buffer_reserve(CodeBuffer *cb, u32 extra)
{
    if (cb->size + extra <= cb->capacity) {
        return true;
    }

    u32 new_capacity = cb->capacity;
    while (new_capacity < cb->size + extra) {
        new_capacity *= 2;
    }
    if (new_capacity > TB_MAX_CAPACITY) {
        return false;
    }

    void *mapping =
        mmap(NULL, new_capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) {
        perror("mmap");
        return false;
    }
    memcpy(mapping, cb->code, cb->size);
    munmap(cb->code, cb->capacity);

    cb->code = mapping;
    cb->capacity = new_capacity;
    return true;
}

bool code_buffer_make_executable(CodeBuffer *cb)
{
    if (mprotect(cb->code, cb->capacity, PROT_READ | PROT_EXEC) != 0) {
        perror("mprotect");
        return false;
    }
    __builtin___clear_cache((char *)cb->code, (char *)cb->code + cb->size);
    return true;
}
