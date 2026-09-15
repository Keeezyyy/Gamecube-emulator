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

// w0 rS_num
// w1 rA_num
// w2 d
extern void emit_stw(u32 **start, u32 **end);

// w0 rS_num
// w1 rA_num
// w2 d
extern void emit_stwu(u32 **start, u32 **end);

extern void emit_oris(u32 **start, u32 **end);

extern void emit_mtspr(u32 **start, u32 **end);

extern void emit_lwz(u32 **start, u32 **end);
extern void emit_bx(u32 **start, u32 **end);
extern void emit_bcx(u32 **start, u32 **end);

extern void emit_rlwinm(u32 **start, u32 **end);
extern void emit_rlwinm_cr0_set(u32 **start, u32 **end);

extern void emit_cpmli(u32 **start, u32 **end);
extern void emit_crxor(u32 **start, u32 **end);
extern void emit_blr(u32 **start, u32 **end);
extern void emit_orx(u32 **start, u32 **end);
extern void emit_norx(u32 **start, u32 **end);
extern void set_cr0_from_w15(u32 **start, u32 **end);
extern void emit_addx(u32 **start, u32 **end);
extern void emit_mfmsr(u32 **start, u32 **end);
extern void emit_addic(u32 **start, u32 **end);
extern void emit_addic_cr0(u32 **start, u32 **end);

extern void set_xer_ca_from_w15(u32 **start, u32 **end);
extern void set_xer_ov_from_w15(u32 **start, u32 **end);
