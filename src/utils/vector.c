#include "vector.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static u32 _get_num_of_bytes(Vector *self)
{
    return self->num_of_bytes;
}
static u32 _get_cap(Vector *self)
{
    return self->cap_in_bytes;
}

static void *_grow(Vector *self)
{
    const void *p = calloc(self->cap_in_bytes * 2, 1);

    if (!p) {
        assert(!"vector grow failed calloc\n");
    }

    memcpy(p, self->buffer, self->num_of_bytes);

    free(self->buffer);

    self->buffer = p;
    self->cap_in_bytes *= 2;
}

void init_Vector(Vector *self, u32 initial_cap)
{
    const void *p = calloc(initial_cap, 1);

    if (!p) {
        assert(!"vector init failed calloc\n");
    }
    self->buffer = p;
    self->cap_in_bytes = initial_cap;
    self->num_of_bytes = 0;

    self->get_cap = &_get_cap;
    self->get_num_of_bytes = &_get_num_of_bytes;
    self->grow = &_grow;
}
