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
  //irp reg, d0, d1, d2, d3, d4, d5, d6, d7, d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31
    //PUSH_64 \reg
  //endr  
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
  //irp reg, d0, d1, d2, d3, d4, d5, d6, d7, d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31
    //POP_64 \reg
  //endr  
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
