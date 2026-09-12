#pragma once

// w0 regA_num
// w1 regD_num
// w2 imm
extern void emit_addi(u32 **start,
                      u32 **end); // imm val is on the stack
extern void emit_ori(u32 **start,
                     u32 **end); // imm val is on the stack

// x0 -> msr_ptr
// w1 -> rS_num
extern void emit_mtmsr(u32 **start, u32 **end);

// w0 -> rD_num
// x1 -> spr_ptr
// w2 -> spr_num
extern void emit_mfspr(u32 **start, u32 **end);
