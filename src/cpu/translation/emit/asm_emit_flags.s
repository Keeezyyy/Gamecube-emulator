GUEST_REGISTER_POINTER .req x19
FUNCTION_ARRAY_POINTER .req x20

.macro PUSH_64 reg
  str \reg, [sp, #-16]!
.endm

.macro POP_64 reg
  ldr \reg, [sp], #16
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




.globl _set_cr0_from_w15
_set_cr0_from_w15:
          adr x2, _set_cr0_from_w15_start
          adr x3, _set_cr0_from_w15_after
          str x2, [x0]
          str x3, [x1]
          ret          
          //x5 -> cr_ptr
          //x16 -> xer_ptr
          //w15 -> val
        _set_cr0_from_w15_start:
          ldr  w7, [x5]                 

          cmp  w15, wzr                  
          cset w8,  lt                  
          cset w9,  gt                  
          cset w10, eq                  

          ldr  w11, [x16]
          lsr  w11, w11, #31

          and  w7, w7, #0x0FFFFFFF      
          orr  w7, w7, w8,  lsl #31
          orr  w7, w7, w9,  lsl #30
          orr  w7, w7, w10, lsl #29
          orr  w7, w7, w11, lsl #28
          str  w7, [x5]        
        _set_cr0_from_w15_after:





.globl _set_xer_ov_from_w15
_set_xer_ov_from_w15:
          adr x2, _set_xer_ov_from_w15_start
          adr x3, _set_xer_ov_from_w15_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _set_xer_ov_from_w15_start:

          cset w8,  vs // overflow
          ldr  w11, [x16]
          mov w9, w8
          lsl w8,w8, 30
          lsl w9,w9, 31

          mov  w12, #0x40000000
          bic w11,w11,w12 
          orr w11, w11, w8
          orr w11, w11, w9
          str  w11, [x16]        
        _set_xer_ov_from_w15_after:


.globl _set_xer_ca_from_w15
_set_xer_ca_from_w15:
          adr x2, _set_xer_ca_from_w15_start
          adr x3, _set_xer_ca_from_w15_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _set_xer_ca_from_w15_start:

          cset w8,  cs // carry out
          ldr  w11, [x16]
          lsl  w8, w8, #29
          bic  w11, w11, #0x20000000   
          orr  w11, w11, w8
          str  w11, [x16]        
        _set_xer_ca_from_w15_after:




.globl _emit_sc
_emit_sc:
          adr x2, _emit_sc_start
          adr x3, _emit_sc_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_sc_start:
        mov w1, 8 //switch to exception function index
        CALL_HELPER_FUNCTION w1

        ret
        _emit_sc_after:

.globl _emit_rfi
_emit_rfi:
          adr x2, _emit_rfi_start
          adr x3, _emit_rfi_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_rfi_start:
        mov w1,11
        CALL_HELPER_FUNCTION w1

        ret
        _emit_rfi_after:

