#include "vector.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    return p;
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

    self->grow = &_grow;
}
