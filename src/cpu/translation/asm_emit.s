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


// w0 -> reg_num
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


// w0 rS_num
// w1 rA_num
// w2 uimm
.globl _emit_oris
_emit_oris:
          adr x2, _emit_oris_start
          adr x3, _emit_oris_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_oris_start:
        LOAD_REGISTER w0, w4
        orr w5, w4, w2
        STORE_REGISTER w1, w5
        _emit_oris_after:




// w0 -> reg_num
// x1 -> spr_ptr
// w2 -> spr_num
.globl _emit_mtspr
_emit_mtspr:
          adr x2, _emit_mtspr_start
          adr x3, _emit_mtspr_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_mtspr_start:
          LOAD_REGISTER w0, w4
          str w4, [x1, w2, uxtw 2] 
        _emit_mtspr_after:




.globl _emit_lwz
_emit_lwz:
          adr x2, _emit_lwz_start
          adr x3, _emit_lwz_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_lwz_start:
        cbnz w1, _emit_lwz_else
        mov w3, 0
        b _emit_lwz_finally
      _emit_lwz_else:
        LOAD_REGISTER w1, w3
      _emit_lwz_finally:

      mov w5, w0
      add w0, w3, w2

      mov w1, 1
      PUSH_32 w0
      PUSH_32 w5
      CALL_HELPER_FUNCTION w1 // return val is in w8
      mov w8, w0
      POP_32 w5
      POP_32 w0
      STORE_REGISTER w5, w8
      _emit_lwz_after:




.globl _emit_bx
_emit_bx:
          adr x2, _emit_bx_start
          adr x3, _emit_bx_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_bx_start:
        str w0, [x1]

      _emit_bx_after:



//w0 regS_num
//w1 regA_num
//w2 SH
//w3 MB
//w4 ME
//w5 RC 
.globl _emit_rlwinm
_emit_rlwinm:
          adr x2, _emit_rlwinm_start
          adr x3, _emit_rlwinm_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_rlwinm_start:
          LOAD_REGISTER w0, w6            

          neg  w7, w2                     
          ror  w6, w6, w7                 

          mov  w8,  #-1                   
          lsr  w9,  w8, w3                
          mov  w10, #31
          sub  w10, w10, w4
          lsl  w10, w8, w10               

          cmp  w3, w4
          and  w11, w9, w10               
          orr  w12, w9, w10                         
          csel w9, w11, w12, le

          and  w6, w6, w9
          STORE_REGISTER w1, w6
      _emit_rlwinm_after:



//w0 regS_num
//w1 regA_num
//w2 SH
//w3 MB
//w4 ME
//w5 RC 
.globl _emit_rlwinm_cr0_set
_emit_rlwinm_cr0_set:
          adr x2, _emit_rlwinm_flag_set_start
          adr x3, _emit_rlwinm_flag_set_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_rlwinm_flag_set_start:

        ldr w7, [x5] 
        and w7, w7, #(1<<3)
        cbnz w6, _emit_rlwinm_flag_set_else
        orr w7, w7, #(1<<2)
        b _emit_rlwinm_flag_set_finally

        _emit_rlwinm_flag_set_else:

        tbz  w6, #31, _emit_rlwinm_flag_set_positiv
        _emit_rlwinm_flag_set_negativ:
        orr w7, w7, #(1<<0)
        b _emit_rlwinm_flag_set_finally

        _emit_rlwinm_flag_set_positiv:

        orr w7, w7, #(1<<1)

        _emit_rlwinm_flag_set_finally:

        str w7, [x5]

        _emit_rlwinm_flag_set_after:




