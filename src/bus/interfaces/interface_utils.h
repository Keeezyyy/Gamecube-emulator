#pragma once

#include <stddef.h>
#define W1C(val, bit_i) (~(val & ((1 << bit_i))))

void set_register(u32 *r, u32 val, int *w1cs, size_t w1cs_length);
