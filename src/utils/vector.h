#pragma once

typedef struct Vector Vector;

struct Vector {
    u32 num_of_bytes;
    u32 cap_in_bytes;
    void *buffer;

    u32 (*get_num_of_bytes)(Vector *);
    u32 (*get_cap)(Vector *);
    void *(*grow)(Vector *);
};

void init_Vector(Vector *self, u32 initial_cap);
