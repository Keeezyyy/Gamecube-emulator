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
        mov w7, 0

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



.globl _emit_rlwinm_cr0_set
_emit_rlwinm_cr0_set:
          adr x2, _emit_rlwinm_flag_set_start
          adr x3, _emit_rlwinm_flag_set_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_rlwinm_flag_set_start:
          ldr  w7, [x5]                 

          cmp  w6, wzr                  
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
          _emit_rlwinm_flag_set_after:




.globl _emit_cpmli
_emit_cpmli:
          adr x2, _emit_cmpli_start
          adr x3, _emit_cmpli_after
          str x2, [x0]
          str x3, [x1]
          ret
        _emit_cmpli_start:
          LOAD_REGISTER w2, w5          
          ldr  w9, [x4]                 

          mov  w7, #28
          sub  w7, w7, w0, lsl #2       

          cmp  w5, w3                   
          cset w10, lo                  
          cset w11, hi                  
          cset w12, eq                  

          lsl  w6, w10, #3
          orr  w6, w6, w11, lsl #2
          orr  w6, w6, w12, lsl #1

          ldr  w13, [x16]
          lsr  w13, w13, #31
          orr  w6, w6, w13              

          mov  w8, #0xF
          lsl  w8, w8, w7
          lsl  w6, w6, w7
          bic  w9, w9, w8               
          orr  w9, w9, w6
          str  w9, [x4]
        _emit_cmpli_after:



.globl _emit_bcx
_emit_bcx:
          adr x2, _emit_bcx_start
          adr x3, _emit_bcx_after
          str x2, [x0]
          str x3, [x1]
          ret          
       _emit_bcx_start:
        ubfx w6, w0, #2, #1         // BO[2]
        ldr  w7, [x15]              // CTR
        cbnz w6, 1f
        sub  w7, w7, #1
        str  w7, [x15]
1:      cmp  w7, #0
        cset w8, ne                 // CTR != 0
        ubfx w9, w0, #1, #1         // BO[3]
        eor  w8, w8, w9
        orr  w6, w6, w8             // ctr_ok

        ldr  w10, [x5]              // CR
        lsr  w10, w10, w1           // w1 = 31 - BI
        and  w10, w10, #1           // CR[BI]
        ubfx w9, w0, #3, #1         // BO[1]
        eor  w10, w10, w9
        eor  w10, w10, #1           // XNOR  ->  CR[BI] == BO[1]
        ubfx w11, w0, #4, #1        // BO[0]
        orr  w10, w10, w11          // cond_ok

        cbz  w3, 2f
        add  w9, w12, #4
        str  w9, [x14]

2:      cmp  w10, #0
        ccmp w6, #0, #4, ne
        b.eq 3f
        cmp  w2, #1
        b.ne 4f
        str  w4, [x13]              // AA=1: NIA = EXTS(BD||00)
        b    _emit_bcx_after
4:      add  w9, w4, w12            // AA=0: NIA = CIA + EXTS(BD||00)
        str  w9, [x13]
        ret
3:      add  w9, w12, #4            // nicht genommen
        str  w9, [x13]
        ret
_emit_bcx_after:



.globl _emit_crxor
_emit_crxor:
          adr x2, _emit_crxor_start
          adr x3, _emit_crxor_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_crxor_start:
        ldr w4, [x3] // w4 = (cr)
        lsr w5, w4, w1
        and w5, w5, 1
        lsr w6, w4, w2
        and w6, w6, 1
        eor w5, w5, w6
        lsl w5, w5, w0
        mov  w7, #1

        
        lsl  w7, w7, w0


        bic  w4, w4, w7 
        orr w4, w4, w5
        str w4, [x3]
        _emit_crxor_after:



.globl _emit_orx
_emit_orx:
          adr x2, _emit_orx_start
          adr x3, _emit_orx_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_orx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        orr w15, w3, w4
        STORE_REGISTER w1, w15
        _emit_orx_after:





.globl _emit_addx
_emit_addx:
          adr x2, _emit_addx_start
          adr x3, _emit_addx_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_addx_start:
        LOAD_REGISTER w1, w3
        LOAD_REGISTER w2, w4
        adds w15, w3, w4
        STORE_REGISTER w0, w15
        _emit_addx_after:







.globl _emit_blr
_emit_blr:
          adr x2, _emit_blr_start
          adr x3, _emit_blr_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_blr_start:
        ldr w2, [x1]
        str w2, [x0]
        ret
        _emit_blr_after:


.globl _emit_mfmsr
_emit_mfmsr:
          adr x2, _emit_mfmsr_start
          adr x3, _emit_mfmsr_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_mfmsr_start:
        ldr w2, [x1]
        STORE_REGISTER w0, w2
        _emit_mfmsr_after:

.globl _emit_norx
_emit_norx:
          adr x2, _emit_norx_start
          adr x3, _emit_norx_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_norx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        orr w15, w3, w4
        mvn w15, w15
        STORE_REGISTER w1, w15
        _emit_norx_after:


.globl _emit_addic
_emit_addic:
          //void emit_addic(*void start, *void end)  // imm val is on the stack 
          adr x22, _emit_addic_start
          adr x23, _emit_addic_after
          STR x22, [x0]
          STR x23, [x1]

          ret
        _emit_addic_start:
          ldr w3, [GUEST_REGISTER_POINTER, w0, uxtw 2] // w19 A reg
          add w15, w3, w1
          STORE_REGISTER w1, w15 
_emit_addic_after:





