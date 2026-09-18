GUEST_REGISTER_POINTER .req x19
FUNCTION_ARRAY_POINTER .req x20

.macro PUSH_64 reg
  str \reg, [sp, #-16]!
.endm

.macro POP_64 reg
  ldr \reg, [sp], #16
.endm

.macro PUSH_32 reg
  str \reg, [sp, #-16]!
.endm

.macro POP_32 reg
  ldr \reg, [sp], #16
.endm


.macro LOAD_REGISTER reg_num_hold, reg_destination
  ldr \reg_destination, [GUEST_REGISTER_POINTER, \reg_num_hold, uxtw #2]
.endm


.macro STORE_REGISTER reg_num_hold, reg_val
    str \reg_val, [GUEST_REGISTER_POINTER, \reg_num_hold, uxtw 2] 
.endm



.macro CALL_HELPER_FUNCTION function_num_register
  stp q0, q1, [sp, #-32]!
  stp q2, q3, [sp, #-32]!
  stp q4, q5, [sp, #-32]!
  stp q6, q7, [sp, #-32]!
  stp q8, q9, [sp, #-32]!
  stp q10, q11, [sp, #-32]!
  stp q12, q13, [sp, #-32]!
  stp q14, q15, [sp, #-32]!
  stp q16, q17, [sp, #-32]!
  stp q18, q19, [sp, #-32]!
  stp q20, q21, [sp, #-32]!
  stp q22, q23, [sp, #-32]!
  stp q24, q25, [sp, #-32]!
  stp q26, q27, [sp, #-32]!
  stp q28, q29, [sp, #-32]!
  stp q30, q31, [sp, #-32]!
  PUSH_64 GUEST_REGISTER_POINTER

  PUSH_64 FUNCTION_ARRAY_POINTER
  stp  x29, x30, [sp, #-16]!   
  mov  x29, sp
  sub sp, sp, #32

  ldr FUNCTION_ARRAY_POINTER, [FUNCTION_ARRAY_POINTER, \function_num_register, uxtw 3]
  blr FUNCTION_ARRAY_POINTER

  add sp, sp, #32
  ldp x29, x30, [sp], #16

  POP_64 FUNCTION_ARRAY_POINTER
  POP_64 GUEST_REGISTER_POINTER

  ldp q30, q31, [sp], #32
  ldp q28, q29, [sp], #32
  ldp q26, q27, [sp], #32
  ldp q24, q25, [sp], #32
  ldp q22, q23, [sp], #32
  ldp q20, q21, [sp], #32
  ldp q18, q19, [sp], #32
  ldp q16, q17, [sp], #32
  ldp q14, q15, [sp], #32
  ldp q12, q13, [sp], #32
  ldp q10, q11, [sp], #32
  ldp q8, q9, [sp], #32
  ldp q6, q7, [sp], #32
  ldp q4, q5, [sp], #32
  ldp q2, q3, [sp], #32
  ldp q0, q1, [sp], #32
.endm


.macro SINGLE_TO_DOUBLE
        lsr  w5, w0, #31
        ubfx w6, w0, #23, #8
        and  w7, w0, #0x7fffff
        lsl  x5, x5, #63
        cbz  w6, 8f

        mov  w9, #0x7ff
        add  w6, w6, #896
        cmp  w6, #1151
        csel w6, w9, w6, eq
        orr  x0, x5, x6, lsl #52
        orr  x0, x0, x7, lsl #29
        b    9f
8:
        mov  x0, x5
        cbz  w7, 9f
        clz  w8, w7
        mov  w9, #31
        sub  w8, w9, w8
        mov  w9, #1
        lsl  w9, w9, w8
        sub  w7, w7, w9
        mov  w9, #52
        sub  w9, w9, w8
        lsl  x7, x7, x9
        add  w8, w8, #874
        orr  x0, x5, x8, lsl #52
        orr  x0, x0, x7
9:
.endm


.macro DOUBLE_TO_SINGLE
        lsr  x5, x0, #63
        ubfx x6, x0, #52, #11
        and  x7, x0, #0xfffffffffffff
        cmp  x6, #896
        b.hi 8f
        cbnz x6, 7f
        cbz  x7, 8f
        b    6f
7:
        cmp  x6, #874
        b.lo 6f
        orr  x7, x7, #0x10000000000000
        mov  w9, #926
        sub  w9, w9, w6
        lsr  x7, x7, x9
        and  w0, w7, #0x7fffff
        orr  w0, w0, w5, lsl #31
        b    9f
6:
        lsl  w0, w5, #31
        b    9f
8:
        ubfx w8, w6, #10, #1
        and  w9, w6, #0x7f
        orr  w8, w9, w8, lsl #7
        lsr  x7, x7, #29
        orr  w0, w7, w8, lsl #23
        orr  w0, w0, w5, lsl #31
9:
.endm


.globl _emit_lfd
_emit_lfd:
          adr x22, _emit_lfd_start
          adr x23, _emit_lfd_after
          STR x22, [x0]
          STR x23, [x1]
          ret

        // in:  w1 = rA, w2 = EXTS(d), x4 = &cpu->fpu.fpr[fD]
        // out: x0 = geladenes Doppelwort (steht zusaetzlich in fpr[fD])
        _emit_lfd_start:
        cbnz w1, lfd_1
        mov w0, 0
        b lfd_2
lfd_1:
        LOAD_REGISTER w1, w0
lfd_2:
        add w0, w0, w2 

        PUSH_64 x4
        mov w2, 2
        CALL_HELPER_FUNCTION w2
        POP_64 x4

        str x0, [x4]
      _emit_lfd_after:

.globl _emit_stfd
_emit_stfd:
          adr x22, _emit_stfd_start
          adr x23, _emit_stfd_after
          STR x22, [x0]
          STR x23, [x1]
          ret

        _emit_stfd_start:
        cbnz w1, stfd_1
        mov w0, 0
        b stfd_2
stfd_1:
        LOAD_REGISTER w1, w0
stfd_2:
        add w0, w0, w2

        mov x1, x4
        mov w2, 7
        CALL_HELPER_FUNCTION w2
      _emit_stfd_after:

.global _copy_fpscr_to_cr1
_copy_fpscr_to_cr1:
          adr x22, _emit_copy_fpscr_to_cr1_start
          adr x23, _emit_copy_fpscr_to_cr1_after
          STR x22, [x0]
          STR x23, [x1]
          ret
_emit_copy_fpscr_to_cr1_start:
      ldr w10, [x15] 
      bic w10, w10, #0x0F000000
      ldr w11, [x16] 
      and w11, w11, 0xF0000000
      lsr w11, w11, 4
      orr w10, w10, w11
      str w10, [x15]
_emit_copy_fpscr_to_cr1_after:


.globl _emit_lfdu
_emit_lfdu:
          adr x22, _emit_lfdu_start
          adr x23, _emit_lfdu_after
          STR x22, [x0]
          STR x23, [x1]
          ret          
        _emit_lfdu_start:
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x4
        PUSH_64 x0
        PUSH_64 x1
        mov w2, 2
        CALL_HELPER_FUNCTION w2
        POP_64 x1
        POP_64 x5
        POP_64 x4
        str x0, [x4]
        STORE_REGISTER w1, w5
        _emit_lfdu_after:


.globl _emit_lfdx
_emit_lfdx:
          adr x22, _emit_lfdx_start
          adr x23, _emit_lfdx_after
          STR x22, [x0]
          STR x23, [x1]
          ret          
        _emit_lfdx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, lfdx_1
        mov w0, 0
        b lfdx_2
lfdx_1:
        LOAD_REGISTER w1, w0
lfdx_2:
        add w0, w0, w2
        PUSH_64 x4
        mov w2, 2
        CALL_HELPER_FUNCTION w2
        POP_64 x4
        str x0, [x4]
        _emit_lfdx_after:


.globl _emit_lfdux
_emit_lfdux:
          adr x22, _emit_lfdux_start
          adr x23, _emit_lfdux_after
          STR x22, [x0]
          STR x23, [x1]
          ret          
        _emit_lfdux_start:
        LOAD_REGISTER w2, w2
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x4
        PUSH_64 x0
        PUSH_64 x1
        mov w2, 2
        CALL_HELPER_FUNCTION w2
        POP_64 x1
        POP_64 x5
        POP_64 x4
        str x0, [x4]
        STORE_REGISTER w1, w5
        _emit_lfdux_after:


.globl _emit_stfdu
_emit_stfdu:
          adr x22, _emit_stfdu_start
          adr x23, _emit_stfdu_after
          STR x22, [x0]
          STR x23, [x1]
          ret          
        _emit_stfdu_start:
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x1
        PUSH_64 x0
        mov x1, x4
        mov w2, 7
        CALL_HELPER_FUNCTION w2
        POP_64 x0
        POP_64 x1
        STORE_REGISTER w1, w0
        _emit_stfdu_after:


.globl _emit_stfdx
_emit_stfdx:
          adr x22, _emit_stfdx_start
          adr x23, _emit_stfdx_after
          STR x22, [x0]
          STR x23, [x1]
          ret          
        _emit_stfdx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, stfdx_1
        mov w0, 0
        b stfdx_2
stfdx_1:
        LOAD_REGISTER w1, w0
stfdx_2:
        add w0, w0, w2
        mov x1, x4
        mov w2, 7
        CALL_HELPER_FUNCTION w2
        _emit_stfdx_after:


.globl _emit_stfdux
_emit_stfdux:
          adr x22, _emit_stfdux_start
          adr x23, _emit_stfdux_after
          STR x22, [x0]
          STR x23, [x1]
          ret          
        _emit_stfdux_start:
        LOAD_REGISTER w2, w2
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x1
        PUSH_64 x0
        mov x1, x4
        mov w2, 7
        CALL_HELPER_FUNCTION w2
        POP_64 x0
        POP_64 x1
        STORE_REGISTER w1, w0
        _emit_stfdux_after:


.globl _emit_fneg
_emit_fneg:
          adr x22, _emit_fneg_start
          adr x23, _emit_fneg_after
          STR x22, [x0]
          STR x23, [x1]
          ret          
        _emit_fneg_start:
        eor x9, x9, #0x8000000000000000
        _emit_fneg_after:


.globl _emit_fabs
_emit_fabs:
          adr x22, _emit_fabs_start
          adr x23, _emit_fabs_after
          STR x22, [x0]
          STR x23, [x1]
          ret          
        _emit_fabs_start:
        and x9, x9, #0x7fffffffffffffff
        _emit_fabs_after:


.globl _emit_fnabs
_emit_fnabs:
          adr x22, _emit_fnabs_start
          adr x23, _emit_fnabs_after
          STR x22, [x0]
          STR x23, [x1]
          ret          
        _emit_fnabs_start:
        orr x9, x9, #0x8000000000000000
        _emit_fnabs_after:









.globl _emit_lfs
_emit_lfs:
          adr x22, _emit_lfs_start
          adr x23, _emit_lfs_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_lfs_start:
        cbnz w1, lfs_1
        mov w0, 0
        b lfs_2
lfs_1:
        LOAD_REGISTER w1, w0
lfs_2:
        add w0, w0, w2
        PUSH_64 x4
        mov w2, 1
        CALL_HELPER_FUNCTION w2
        POP_64 x4
        SINGLE_TO_DOUBLE
        str x0, [x4]
        _emit_lfs_after:


.globl _emit_lfsu
_emit_lfsu:
          adr x22, _emit_lfsu_start
          adr x23, _emit_lfsu_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_lfsu_start:
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x4
        PUSH_64 x0
        PUSH_64 x1
        mov w2, 1
        CALL_HELPER_FUNCTION w2
        POP_64 x1
        POP_64 x5
        POP_64 x4
        STORE_REGISTER w1, w5
        SINGLE_TO_DOUBLE
        str x0, [x4]
        _emit_lfsu_after:


.globl _emit_lfsx
_emit_lfsx:
          adr x22, _emit_lfsx_start
          adr x23, _emit_lfsx_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_lfsx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, lfsx_1
        mov w0, 0
        b lfsx_2
lfsx_1:
        LOAD_REGISTER w1, w0
lfsx_2:
        add w0, w0, w2
        PUSH_64 x4
        mov w2, 1
        CALL_HELPER_FUNCTION w2
        POP_64 x4
        SINGLE_TO_DOUBLE
        str x0, [x4]
        _emit_lfsx_after:


.globl _emit_lfsux
_emit_lfsux:
          adr x22, _emit_lfsux_start
          adr x23, _emit_lfsux_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_lfsux_start:
        LOAD_REGISTER w2, w2
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x4
        PUSH_64 x0
        PUSH_64 x1
        mov w2, 1
        CALL_HELPER_FUNCTION w2
        POP_64 x1
        POP_64 x5
        POP_64 x4
        STORE_REGISTER w1, w5
        SINGLE_TO_DOUBLE
        str x0, [x4]
        _emit_lfsux_after:


.globl _emit_stfs
_emit_stfs:
          adr x22, _emit_stfs_start
          adr x23, _emit_stfs_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_stfs_start:
        mov x0, x4
        DOUBLE_TO_SINGLE
        mov w3, w0
        cbnz w1, stfs_1
        mov w0, 0
        b stfs_2
stfs_1:
        LOAD_REGISTER w1, w0
stfs_2:
        add w0, w0, w2
        mov w1, w3
        mov w2, 0
        CALL_HELPER_FUNCTION w2
        _emit_stfs_after:


.globl _emit_stfsu
_emit_stfsu:
          adr x22, _emit_stfsu_start
          adr x23, _emit_stfsu_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_stfsu_start:
        mov x0, x4
        DOUBLE_TO_SINGLE
        mov w3, w0
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x0
        PUSH_64 x1
        mov w1, w3
        mov w2, 0
        CALL_HELPER_FUNCTION w2
        POP_64 x1
        POP_64 x0
        STORE_REGISTER w1, w0
        _emit_stfsu_after:


.globl _emit_stfsx
_emit_stfsx:
          adr x22, _emit_stfsx_start
          adr x23, _emit_stfsx_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_stfsx_start:
        LOAD_REGISTER w2, w2
        mov x0, x4
        DOUBLE_TO_SINGLE
        mov w3, w0
        cbnz w1, stfsx_1
        mov w0, 0
        b stfsx_2
stfsx_1:
        LOAD_REGISTER w1, w0
stfsx_2:
        add w0, w0, w2
        mov w1, w3
        mov w2, 0
        CALL_HELPER_FUNCTION w2
        _emit_stfsx_after:


.globl _emit_stfsux
_emit_stfsux:
          adr x22, _emit_stfsux_start
          adr x23, _emit_stfsux_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_stfsux_start:
        LOAD_REGISTER w2, w2
        mov x0, x4
        DOUBLE_TO_SINGLE
        mov w3, w0
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x0
        PUSH_64 x1
        mov w1, w3
        mov w2, 0
        CALL_HELPER_FUNCTION w2
        POP_64 x1
        POP_64 x0
        STORE_REGISTER w1, w0
        _emit_stfsux_after:


.globl _emit_stfiwx
_emit_stfiwx:
          adr x22, _emit_stfiwx_start
          adr x23, _emit_stfiwx_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_stfiwx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, stfiwx_1
        mov w0, 0
        b stfiwx_2
stfiwx_1:
        LOAD_REGISTER w1, w0
stfiwx_2:
        add w0, w0, w2
        mov w1, w4
        mov w2, 0
        CALL_HELPER_FUNCTION w2
        _emit_stfiwx_after:


.globl _emit_psq_l
_emit_psq_l:
          adr x22, _emit_psq_l_start
          adr x23, _emit_psq_l_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_psq_l_start:
        cbnz w1, psq_l_1
        mov w0, 0
        b psq_l_2
psq_l_1:
        LOAD_REGISTER w1, w0
psq_l_2:
        add w0, w0, w2
        ldr w1, [x3]
        mov w2, w4
        mov w5, 9
        CALL_HELPER_FUNCTION w5
        _emit_psq_l_after:


.globl _emit_psq_lu
_emit_psq_lu:
          adr x22, _emit_psq_lu_start
          adr x23, _emit_psq_lu_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_psq_lu_start:
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x0
        PUSH_64 x1
        ldr w1, [x3]
        mov w2, w4
        mov w5, 9
        CALL_HELPER_FUNCTION w5
        POP_64 x2
        POP_64 x3
        STORE_REGISTER w2, w3
        _emit_psq_lu_after:


.globl _emit_psq_lx
_emit_psq_lx:
          adr x22, _emit_psq_lx_start
          adr x23, _emit_psq_lx_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_psq_lx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, psq_lx_1
        mov w0, 0
        b psq_lx_2
psq_lx_1:
        LOAD_REGISTER w1, w0
psq_lx_2:
        add w0, w0, w2
        ldr w1, [x3]
        mov w2, w4
        mov w5, 9
        CALL_HELPER_FUNCTION w5
        _emit_psq_lx_after:


.globl _emit_psq_lux
_emit_psq_lux:
          adr x22, _emit_psq_lux_start
          adr x23, _emit_psq_lux_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_psq_lux_start:
        LOAD_REGISTER w2, w2
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x0
        PUSH_64 x1
        ldr w1, [x3]
        mov w2, w4
        mov w5, 9
        CALL_HELPER_FUNCTION w5
        POP_64 x2
        POP_64 x3
        STORE_REGISTER w2, w3
        _emit_psq_lux_after:


.globl _emit_psq_st
_emit_psq_st:
          adr x22, _emit_psq_st_start
          adr x23, _emit_psq_st_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_psq_st_start:
        cbnz w1, psq_st_1
        mov w0, 0
        b psq_st_2
psq_st_1:
        LOAD_REGISTER w1, w0
psq_st_2:
        add w0, w0, w2
        ldr w1, [x3]
        mov w2, w4
        mov x3, x5
        mov x4, x6
        mov w7, 10
        CALL_HELPER_FUNCTION w7
        _emit_psq_st_after:


.globl _emit_psq_stu
_emit_psq_stu:
          adr x22, _emit_psq_stu_start
          adr x23, _emit_psq_stu_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_psq_stu_start:
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x0
        PUSH_64 x1
        ldr w1, [x3]
        mov w2, w4
        mov x3, x5
        mov x4, x6
        mov w7, 10
        CALL_HELPER_FUNCTION w7
        POP_64 x1
        POP_64 x0
        STORE_REGISTER w1, w0
        _emit_psq_stu_after:


.globl _emit_psq_stx
_emit_psq_stx:
          adr x22, _emit_psq_stx_start
          adr x23, _emit_psq_stx_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_psq_stx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, psq_stx_1
        mov w0, 0
        b psq_stx_2
psq_stx_1:
        LOAD_REGISTER w1, w0
psq_stx_2:
        add w0, w0, w2
        ldr w1, [x3]
        mov w2, w4
        mov x3, x5
        mov x4, x6
        mov w7, 10
        CALL_HELPER_FUNCTION w7
        _emit_psq_stx_after:


.globl _emit_psq_stux
_emit_psq_stux:
          adr x22, _emit_psq_stux_start
          adr x23, _emit_psq_stux_after
          STR x22, [x0]
          STR x23, [x1]
          ret
        _emit_psq_stux_start:
        LOAD_REGISTER w2, w2
        LOAD_REGISTER w1, w0
        add w0, w0, w2
        PUSH_64 x0
        PUSH_64 x1
        ldr w1, [x3]
        mov w2, w4
        mov x3, x5
        mov x4, x6
        mov w7, 10
        CALL_HELPER_FUNCTION w7
        POP_64 x1
        POP_64 x0
        STORE_REGISTER w1, w0
        _emit_psq_stux_after:
