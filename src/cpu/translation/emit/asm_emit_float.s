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
  .irp reg, d0, d1, d2, d3, d4, d5, d6, d7, d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31
    PUSH_64 \reg
  .endr  
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

  .irp reg, d0, d1, d2, d3, d4, d5, d6, d7, d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31
    POP_64 \reg
  .endr  
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

        PUSH_64 x0
        PUSH_64 x4
        lsr x1, x4, #32
        mov w2, 0
        CALL_HELPER_FUNCTION w2
        POP_64 x4
        POP_64 x0

        add w0, w0, #4
        mov w1, w4
        mov w2, 0
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
        PUSH_64 x4
        lsr x1, x4, #32
        mov w2, 0
        CALL_HELPER_FUNCTION w2
        POP_64 x4
        POP_64 x0
        PUSH_64 x0
        add w0, w0, #4
        mov w1, w4
        mov w2, 0
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
        PUSH_64 x0
        PUSH_64 x4
        lsr x1, x4, #32
        mov w2, 0
        CALL_HELPER_FUNCTION w2
        POP_64 x4
        POP_64 x0
        add w0, w0, #4
        mov w1, w4
        mov w2, 0
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
        PUSH_64 x4
        lsr x1, x4, #32
        mov w2, 0
        CALL_HELPER_FUNCTION w2
        POP_64 x4
        POP_64 x0
        PUSH_64 x0
        add w0, w0, #4
        mov w1, w4
        mov w2, 0
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







