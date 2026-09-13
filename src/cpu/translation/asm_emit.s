GUEST_REGISTER_POINTER .req x16
FUNCTION_ARRAY_POINTER .req x15

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


.macro CALL_HELPER_FUNCTION function_num_register
  PUSH_64 FUNCTION_ARRAY_POINTER
  stp  x29, x30, [sp, #-16]!   
  mov  x29, sp
  sub sp, sp, #32

  ldr FUNCTION_ARRAY_POINTER, [FUNCTION_ARRAY_POINTER, \function_num_register, uxtw 3]
  blr FUNCTION_ARRAY_POINTER

  add sp, sp, #32
  ldp x29, x30, [sp], #16

  POP_64 FUNCTION_ARRAY_POINTER
.endm




.globl _emit_addi
_emit_addi:
          //void emit_addi(*void start, *void end)  // imm val is on the stack 
          adr x22, _emit_addi_start
          adr x23, _emit_addi_after
          STR x22, [x0]
          STR x23, [x1]

  //GUEST_REGISTER_POINTER register pointer
          //w0 regA_num
          //w1 regD_num
          //w2 imm
          ret
        _emit_addi_start:
          ldr w3, [GUEST_REGISTER_POINTER, w0, uxtw 2] // w19 A reg

          cbnz w3, _emit_addi_start_else
          STR w2, [GUEST_REGISTER_POINTER, w1, uxtw 2]
          B _emit_addi_after
        _emit_addi_start_else:
          add w4, w3, w2
          STR w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        _emit_addi_after:

//w0 regS_num
//w1 regA_num
//w2 uimm
.globl _emit_ori
_emit_ori:
          adr x2, _emit_ori_start
          adr x3, _emit_ori_after
          str x2, [x0]
          str x3, [x1]
          ret          
          //x0 regS_ptr
          //x1 regA_ptr
          //x2 imm
          ret
        _emit_ori_start:
          mov w3, 0
          ldr w3, [GUEST_REGISTER_POINTER, w0, uxtw 2]
          orr w4, w2, w3
          str w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        _emit_ori_after:


// x0 -> msr_ptr
// w1 -> rS_num
.globl _emit_mtmsr
_emit_mtmsr:
          adr x2, _emit_mtmsr_start
          adr x3, _emit_mtmsr_after
          str x2, [x0]
          str x3, [x1]
          ret          
          //x0 regS_ptr
          //x1 regA_ptr
          //x2 imm
          ret
        _emit_mtmsr_start:
          ldr w2, [GUEST_REGISTER_POINTER, w1, uxtw 2]
          str w2, [x0]
        _emit_mtmsr_after:


// x1 -> spr_ptr
// w2 -> spr_num
.globl _emit_mfspr
_emit_mfspr:
          adr x2, _emit_mfspr_start
          adr x3, _emit_mfspr_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_mfspr_start:
          ldr w3, [x1, w2, uxtw 2]

          str w3, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        _emit_mfspr_after:

// w0 rS_num
// w1 rA_num
// w2 d
.globl _emit_stw
_emit_stw:
          adr x2, _emit_stw_start
          adr x3, _emit_stw_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_stw_start:
        cbnz w1, _emit_stw_else
        mov w4, 0
        b _emit_stw_finaly

        _emit_stw_else:
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        
        _emit_stw_finaly:
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        rev w6, w6
        mov x0, 0
        mov x1, 0
        mov w0, w5
        mov w1, w6

        mov x7, 0
        //str w6, [FUNCTION_ARRAY_POINTER, w5, uxtw 0]
        CALL_HELPER_FUNCTION w7 // _write( u32 adr, u32 val)
        _emit_stw_after:




// w0 rS_num
// w1 rA_num
// w2 d
.globl _emit_stwu
_emit_stwu:
          adr x2, _emit_stwu_start
          adr x3, _emit_stwu_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_stwu_start:

        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        
        _emit_stwu_finaly:
        add w5, w4, w2

        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        rev w6, w6

        mov w8, w1

        mov w0, w5
        mov w1, w6
        PUSH_32 w5
        PUSH_32 w8
        CALL_HELPER_FUNCTION w7 // _write( u32 adr, u32 val) // str    w6, [FUNCTION_ARRAY_POINTER, w5, uxtw]
        POP_32 w8
        POP_32 w5

        str w5, [GUEST_REGISTER_POINTER, w8, uxtw 2]
        _emit_stwu_after:

