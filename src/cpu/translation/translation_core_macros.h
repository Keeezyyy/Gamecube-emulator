#pragma once

#define GUEST_REG(v) (v + 32)

#define GET_REG_AT_RANGE(a, b) (_get_ext_from_instruction(instruction, a, b))
