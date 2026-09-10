#pragma once

bool encode_logical_imm(uint64_t mask, uint32_t *N, uint32_t *immr, uint32_t *imms);

typedef struct {
    uint32_t N;
    uint32_t immr;
    uint32_t imms;
} a64_logical_imm;

bool a64_encode_bitmask(uint64_t imm, int regsize, a64_logical_imm *out);
