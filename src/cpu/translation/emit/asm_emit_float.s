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
.endm


.globl _emit_lfd
_emit_lfd:
          adr x22, _emit_lfd_start
          adr x23, _emit_lfd_after
          STR x22, [x0]
          STR x23, [x1]
          ret

        _emit_lfd_start:
        cbnz w1, lfd_1
        mov w3, 0
        b lfd_2
lfd_1:
        LOAD_REGISTER w1, w3
lfd_2:
        add w3, w3, w2

        mov w2, 2//func num
        CALL_HELPER_FUNCTION w2
      _emit_lfd_after:


