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

//TODO: change endianess in asm code rather than in bus.c 
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




.globl _emit_sth
_emit_sth:
          adr x2, _emit_sth_start
          adr x3, _emit_sth_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_sth_start:
        cbnz w1, _emit_sth_else
        mov w4, 0
        b _emit_sth_finaly

        _emit_sth_else:
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        
        _emit_sth_finaly:
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w0, w5
        and w1, w6, 0xffff

        mov x7, 5
        CALL_HELPER_FUNCTION w7
        _emit_sth_after:

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



.globl _emit_cmpi
_emit_cmpi:
          adr x2, _emit_cmpi_start
          adr x3, _emit_cmpi_after
          str x2, [x0]
          str x3, [x1]
          ret
        _emit_cmpi_start:
          LOAD_REGISTER w2, w5          
          ldr  w9, [x4]                 

          mov  w7, #28
          sub  w7, w7, w0, lsl #2       

          cmp  w5, w3                   
          cset w10, lt                  
          cset w11, gt                  
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
        _emit_cmpi_after:



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
        ret
4:      add  w9, w4, w12            // AA=0: NIA = CIA + EXTS(BD||00)
        str  w9, [x13]
        ret
3:      add  w9, w12, #4            // nicht genommen
        str  w9, [x13]
        ret
_emit_bcx_after:


.globl _emit_bcx_jump_in_tb
_emit_bcx_jump_in_tb:
          adr x2, _emit_bcx_jump_in_tb_start
          adr x3, _emit_bcx_jump_in_tb_after
          str x2, [x0]
          str x3, [x1]
          ret          
       _emit_bcx_jump_in_tb_start:
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
        br x16
4:      add  w9, w4, w12            // AA=0: NIA = CIA + EXTS(BD||00)
        str  w9, [x13]
        br x16
3:      add  w9, w12, #4            // nicht genommen
        str  w9, [x13]
_emit_bcx_jump_in_tb_after:




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


.globl _emit_addex
_emit_addex:
          adr x2, _emit_addex_start
          adr x3, _emit_addex_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_addex_start:
        LOAD_REGISTER w1, w3
        LOAD_REGISTER w2, w4
        ldr  w11, [x16]
          lsr  w11, w11, #29
          and w11, w11, 1
        cmp w11, #1
        adcs w15, w3, w4
        STORE_REGISTER w0, w15
        _emit_addex_after:







.globl _emit_blr
_emit_blr:
          adr x2, _emit_blr_start
          adr x3, _emit_blr_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_blr_start:
        ldr w2, [x1]
        bic w2, w2, #3
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
          adds w15, w3, w2
          STORE_REGISTER w1, w15 
_emit_addic_after:




.globl _emit_bclr
_emit_bclr:
          adr x2, _emit_bclr_start
          adr x3, _emit_bclr_after
          str x2, [x0]
          str x3, [x1]
          ret          
       _emit_bclr_start:
        ubfx w6, w0, #2, #1         // BO[2]
        ldr  w7, [x15]              // CTR
        cbnz w6, bclr_1
        sub  w7, w7, #1
        str  w7, [x15]
bclr_1:      cmp  w7, #0
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


bclr_2:      
        ldr  w9, [x14]              
        cbz  w3, bclr_4
        add  w11, w12, #4           
        str  w11, [x14]            
bclr_4:
        cmp  w10, #0
        ccmp w6, #0, #4, ne
        b.eq bclr_3
        bic  w9, w9, #3
        str  w9, [x13]
        ret

bclr_3:      add  w9, w12, #4            // nicht genommen
        str  w9, [x13]
        ret
_emit_bclr_after:


.globl _emit_cmp
_emit_cmp:
          adr x2, _emit_cmp_start
          adr x3, _emit_cmp_after
          str x2, [x0]
          str x3, [x1]
          ret
        _emit_cmp_start:
          LOAD_REGISTER w2, w5          
          LOAD_REGISTER w3, w15          
          mov w3, w15
          ldr  w9, [x4]                 

          mov  w7, #28
          sub  w7, w7, w0, lsl #2       

          cmp  w5, w3                   
          cset w10, lt                  
          cset w11, gt                  
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
        _emit_cmp_after:


// w0 rS_num
// w1 rA_num
// w2 d
.globl _emit_stmw
_emit_stmw:
          adr x2, _emit_stmw_start
          adr x3, _emit_stmw_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_stmw_start:
        cbnz w1, stmw_1
        mov w5, 0
        b stmw_2
stmw_1:
        LOAD_REGISTER w1, w5
stmw_2:
        add w6, w5, w2

        mov w5, w0
stmw_3:
        cmp w5, 31
        b.gt _emit_stmw_after

        LOAD_REGISTER w5,w1 
        PUSH_32 w5
        PUSH_32 w1
        PUSH_32 w6
        mov w0, w6
        mov w2, 0
        CALL_HELPER_FUNCTION w2 
        POP_32 w6
        POP_32 w1
        POP_32 w5
        add w5, w5, 1
        add w6, w6, 4
        b stmw_3

        _emit_stmw_after:




.globl _emit_andx
_emit_andx:
          adr x2, _emit_andx_start
          adr x3, _emit_andx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_andx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        and w15, w3, w4
        STORE_REGISTER w1, w15
        _emit_andx_after:


.globl _emit_andcx
_emit_andcx:
          adr x2, _emit_andcx_start
          adr x3, _emit_andcx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_andcx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        bic w15, w3, w4
        STORE_REGISTER w1, w15
        _emit_andcx_after:


.globl _emit_orcx
_emit_orcx:
          adr x2, _emit_orcx_start
          adr x3, _emit_orcx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_orcx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        orn w15, w3, w4
        STORE_REGISTER w1, w15
        _emit_orcx_after:


.globl _emit_xorx
_emit_xorx:
          adr x2, _emit_xorx_start
          adr x3, _emit_xorx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_xorx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        eor w15, w3, w4
        STORE_REGISTER w1, w15
        _emit_xorx_after:


.globl _emit_nandx
_emit_nandx:
          adr x2, _emit_nandx_start
          adr x3, _emit_nandx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_nandx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        and w15, w3, w4
        mvn w15, w15
        STORE_REGISTER w1, w15
        _emit_nandx_after:


.globl _emit_eqvx
_emit_eqvx:
          adr x2, _emit_eqvx_start
          adr x3, _emit_eqvx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_eqvx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        eon w15, w3, w4
        STORE_REGISTER w1, w15
        _emit_eqvx_after:


.globl _emit_slwx
_emit_slwx:
          adr x2, _emit_slwx_start
          adr x3, _emit_slwx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_slwx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        and x4, x4, #0x3f
        lsl x15, x3, x4
        STORE_REGISTER w1, w15
        _emit_slwx_after:


.globl _emit_srwx
_emit_srwx:
          adr x2, _emit_srwx_start
          adr x3, _emit_srwx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_srwx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        and x4, x4, #0x3f
        lsr x15, x3, x4
        STORE_REGISTER w1, w15
        _emit_srwx_after:


.globl _emit_srawx
_emit_srawx:
          adr x2, _emit_srawx_start
          adr x3, _emit_srawx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_srawx_start:
        LOAD_REGISTER w0, w3
        LOAD_REGISTER w2, w4
        sxtw x3, w3
        and x4, x4, #0x3f
        asr x15, x3, x4
        lsl x6, x15, x4
        cmp x6, x3
        cset w8, ne
        and w8, w8, w3, lsr #31
        cmp w8, #1
        STORE_REGISTER w1, w15
        _emit_srawx_after:


.globl _emit_srawix
_emit_srawix:
          adr x2, _emit_srawix_start
          adr x3, _emit_srawix_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_srawix_start:
        LOAD_REGISTER w0, w3
        sxtw x3, w3
        and x4, x2, #0x1f
        asr x15, x3, x4
        lsl x6, x15, x4
        cmp x6, x3
        cset w8, ne
        and w8, w8, w3, lsr #31
        cmp w8, #1
        STORE_REGISTER w1, w15
        _emit_srawix_after:


.globl _emit_cntlzwx
_emit_cntlzwx:
          adr x2, _emit_cntlzwx_start
          adr x3, _emit_cntlzwx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_cntlzwx_start:
        LOAD_REGISTER w0, w3
        clz w15, w3
        STORE_REGISTER w1, w15
        _emit_cntlzwx_after:


.globl _emit_extsbx
_emit_extsbx:
          adr x2, _emit_extsbx_start
          adr x3, _emit_extsbx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_extsbx_start:
        LOAD_REGISTER w0, w3
        sxtb w15, w3
        STORE_REGISTER w1, w15
        _emit_extsbx_after:


.globl _emit_extshx
_emit_extshx:
          adr x2, _emit_extshx_start
          adr x3, _emit_extshx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_extshx_start:
        LOAD_REGISTER w0, w3
        sxth w15, w3
        STORE_REGISTER w1, w15
        _emit_extshx_after:


.globl _emit_xori
_emit_xori:
          adr x2, _emit_xori_start
          adr x3, _emit_xori_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_xori_start:
        LOAD_REGISTER w0, w3
        eor w15, w3, w2
        STORE_REGISTER w1, w15
        _emit_xori_after:


.globl _emit_andi
_emit_andi:
          adr x2, _emit_andi_start
          adr x3, _emit_andi_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_andi_start:
        LOAD_REGISTER w0, w3
        and w15, w3, w2
        STORE_REGISTER w1, w15
        _emit_andi_after:


.globl _emit_subfx
_emit_subfx:
          adr x2, _emit_subfx_start
          adr x3, _emit_subfx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_subfx_start:
        LOAD_REGISTER w1, w3
        LOAD_REGISTER w2, w4
        subs w15, w4, w3
        STORE_REGISTER w0, w15
        _emit_subfx_after:


.globl _emit_subfex
_emit_subfex:
          adr x2, _emit_subfex_start
          adr x3, _emit_subfex_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_subfex_start:
        LOAD_REGISTER w1, w3
        LOAD_REGISTER w2, w4
        ldr  w11, [x16]
        lsr  w11, w11, #29
        and w11, w11, 1
        cmp w11, #1
        sbcs w15, w4, w3
        STORE_REGISTER w0, w15
        _emit_subfex_after:


.globl _emit_negx
_emit_negx:
          adr x2, _emit_negx_start
          adr x3, _emit_negx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_negx_start:
        LOAD_REGISTER w1, w3
        negs w15, w3
        STORE_REGISTER w0, w15
        _emit_negx_after:


.globl _emit_addzex
_emit_addzex:
          adr x2, _emit_addzex_start
          adr x3, _emit_addzex_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_addzex_start:
        LOAD_REGISTER w1, w3
        ldr  w11, [x16]
        lsr  w11, w11, #29
        and w11, w11, 1
        cmp w11, #1
        adcs w15, w3, wzr
        STORE_REGISTER w0, w15
        _emit_addzex_after:


.globl _emit_addmex
_emit_addmex:
          adr x2, _emit_addmex_start
          adr x3, _emit_addmex_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_addmex_start:
        LOAD_REGISTER w1, w3
        ldr  w11, [x16]
        lsr  w11, w11, #29
        and w11, w11, 1
        cmp w11, #1
        mov w4, #-1
        adcs w15, w3, w4
        STORE_REGISTER w0, w15
        _emit_addmex_after:


.globl _emit_subfzex
_emit_subfzex:
          adr x2, _emit_subfzex_start
          adr x3, _emit_subfzex_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_subfzex_start:
        LOAD_REGISTER w1, w3
        mvn w3, w3
        ldr  w11, [x16]
        lsr  w11, w11, #29
        and w11, w11, 1
        cmp w11, #1
        adcs w15, w3, wzr
        STORE_REGISTER w0, w15
        _emit_subfzex_after:


.globl _emit_subfmex
_emit_subfmex:
          adr x2, _emit_subfmex_start
          adr x3, _emit_subfmex_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_subfmex_start:
        LOAD_REGISTER w1, w3
        mvn w3, w3
        ldr  w11, [x16]
        lsr  w11, w11, #29
        and w11, w11, 1
        cmp w11, #1
        mov w4, #-1
        adcs w15, w3, w4
        STORE_REGISTER w0, w15
        _emit_subfmex_after:


.globl _emit_mullwx
_emit_mullwx:
          adr x2, _emit_mullwx_start
          adr x3, _emit_mullwx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_mullwx_start:
        LOAD_REGISTER w1, w3
        LOAD_REGISTER w2, w4
        smull x15, w3, w4
        sxtw x9, w15
        cmp x15, x9
        cset w9, ne
        mov w10, #0x7fffffff
        adds w10, w10, w9
        STORE_REGISTER w0, w15
        _emit_mullwx_after:


.globl _emit_mulhwx
_emit_mulhwx:
          adr x2, _emit_mulhwx_start
          adr x3, _emit_mulhwx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_mulhwx_start:
        LOAD_REGISTER w1, w3
        LOAD_REGISTER w2, w4
        smull x15, w3, w4
        asr x15, x15, #32
        STORE_REGISTER w0, w15
        _emit_mulhwx_after:


.globl _emit_mulhwux
_emit_mulhwux:
          adr x2, _emit_mulhwux_start
          adr x3, _emit_mulhwux_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_mulhwux_start:
        LOAD_REGISTER w1, w3
        LOAD_REGISTER w2, w4
        umull x15, w3, w4
        lsr x15, x15, #32
        STORE_REGISTER w0, w15
        _emit_mulhwux_after:


.globl _emit_divwx
_emit_divwx:
          adr x2, _emit_divwx_start
          adr x3, _emit_divwx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_divwx_start:
        LOAD_REGISTER w1, w3
        LOAD_REGISTER w2, w4
        sdiv w15, w3, w4
        cmp w4, #0
        cset w9, eq
        mov w10, #0x80000000
        cmp w3, w10
        ccmn w4, #1, #0, eq
        cset w11, eq
        orr w9, w9, w11
        mov w10, #0x7fffffff
        adds w10, w10, w9
        STORE_REGISTER w0, w15
        _emit_divwx_after:


.globl _emit_divwux
_emit_divwux:
          adr x2, _emit_divwux_start
          adr x3, _emit_divwux_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_divwux_start:
        LOAD_REGISTER w1, w3
        LOAD_REGISTER w2, w4
        udiv w15, w3, w4
        cmp w4, #0
        cset w9, eq
        mov w10, #0x7fffffff
        adds w10, w10, w9
        STORE_REGISTER w0, w15
        _emit_divwux_after:


.globl _emit_subfic
_emit_subfic:
          adr x2, _emit_subfic_start
          adr x3, _emit_subfic_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_subfic_start:
        ldr w3, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        subs w15, w2, w3
        STORE_REGISTER w1, w15
        _emit_subfic_after:


.globl _emit_mulli
_emit_mulli:
          adr x2, _emit_mulli_start
          adr x3, _emit_mulli_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_mulli_start:
        ldr w3, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mul w15, w3, w2
        STORE_REGISTER w1, w15
        _emit_mulli_after:


.globl _emit_cmpl
_emit_cmpl:
          adr x2, _emit_cmpl_start
          adr x3, _emit_cmpl_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_cmpl_start:
        LOAD_REGISTER w2, w5
        LOAD_REGISTER w3, w15
        mov w3, w15
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
        _emit_cmpl_after:


.globl _emit_crand
_emit_crand:
          adr x2, _emit_crand_start
          adr x3, _emit_crand_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_crand_start:
        ldr w4, [x3]
        lsr w5, w4, w1
        and w5, w5, 1
        lsr w6, w4, w2
        and w6, w6, 1
        and w5, w5, w6
        lsl w5, w5, w0
        mov  w7, #1
        lsl  w7, w7, w0
        bic  w4, w4, w7
        orr w4, w4, w5
        str w4, [x3]
        _emit_crand_after:


.globl _emit_crandc
_emit_crandc:
          adr x2, _emit_crandc_start
          adr x3, _emit_crandc_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_crandc_start:
        ldr w4, [x3]
        lsr w5, w4, w1
        and w5, w5, 1
        lsr w6, w4, w2
        and w6, w6, 1
        bic w5, w5, w6
        lsl w5, w5, w0
        mov  w7, #1
        lsl  w7, w7, w0
        bic  w4, w4, w7
        orr w4, w4, w5
        str w4, [x3]
        _emit_crandc_after:


.globl _emit_creqv
_emit_creqv:
          adr x2, _emit_creqv_start
          adr x3, _emit_creqv_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_creqv_start:
        ldr w4, [x3]
        lsr w5, w4, w1
        and w5, w5, 1
        lsr w6, w4, w2
        and w6, w6, 1
        eon w5, w5, w6
        and w5, w5, 1
        lsl w5, w5, w0
        mov  w7, #1
        lsl  w7, w7, w0
        bic  w4, w4, w7
        orr w4, w4, w5
        str w4, [x3]
        _emit_creqv_after:


.globl _emit_crnand
_emit_crnand:
          adr x2, _emit_crnand_start
          adr x3, _emit_crnand_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_crnand_start:
        ldr w4, [x3]
        lsr w5, w4, w1
        and w5, w5, 1
        lsr w6, w4, w2
        and w6, w6, 1
        and w5, w5, w6
        mvn w5, w5
        and w5, w5, 1
        lsl w5, w5, w0
        mov  w7, #1
        lsl  w7, w7, w0
        bic  w4, w4, w7
        orr w4, w4, w5
        str w4, [x3]
        _emit_crnand_after:


.globl _emit_crnor
_emit_crnor:
          adr x2, _emit_crnor_start
          adr x3, _emit_crnor_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_crnor_start:
        ldr w4, [x3]
        lsr w5, w4, w1
        and w5, w5, 1
        lsr w6, w4, w2
        and w6, w6, 1
        orr w5, w5, w6
        mvn w5, w5
        and w5, w5, 1
        lsl w5, w5, w0
        mov  w7, #1
        lsl  w7, w7, w0
        bic  w4, w4, w7
        orr w4, w4, w5
        str w4, [x3]
        _emit_crnor_after:


.globl _emit_cror
_emit_cror:
          adr x2, _emit_cror_start
          adr x3, _emit_cror_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_cror_start:
        ldr w4, [x3]
        lsr w5, w4, w1
        and w5, w5, 1
        lsr w6, w4, w2
        and w6, w6, 1
        orr w5, w5, w6
        lsl w5, w5, w0
        mov  w7, #1
        lsl  w7, w7, w0
        bic  w4, w4, w7
        orr w4, w4, w5
        str w4, [x3]
        _emit_cror_after:


.globl _emit_crorc
_emit_crorc:
          adr x2, _emit_crorc_start
          adr x3, _emit_crorc_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_crorc_start:
        ldr w4, [x3]
        lsr w5, w4, w1
        and w5, w5, 1
        lsr w6, w4, w2
        and w6, w6, 1
        orn w5, w5, w6
        and w5, w5, 1
        lsl w5, w5, w0
        mov  w7, #1
        lsl  w7, w7, w0
        bic  w4, w4, w7
        orr w4, w4, w5
        str w4, [x3]
        _emit_crorc_after:


.globl _emit_rlwnm
_emit_rlwnm:
          adr x2, _emit_rlwnm_start
          adr x3, _emit_rlwnm_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_rlwnm_start:
        LOAD_REGISTER w2, w2
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
        _emit_rlwnm_after:


.globl _emit_rlwimi
_emit_rlwimi:
          adr x2, _emit_rlwimi_start
          adr x3, _emit_rlwimi_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_rlwimi_start:
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
        LOAD_REGISTER w1, w13
        bic w13, w13, w9
        orr w6, w6, w13
        STORE_REGISTER w1, w6
        _emit_rlwimi_after:


.globl _emit_lwzu
_emit_lwzu:
          adr x2, _emit_lwzu_start
          adr x3, _emit_lwzu_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lwzu_start:
        LOAD_REGISTER w1, w3
        add w6, w3, w2
        mov w5, w0
        mov w8, w1
        mov w0, w6
        mov w1, 1
        PUSH_32 w5
        PUSH_32 w6
        PUSH_32 w8
        CALL_HELPER_FUNCTION w1
        mov w9, w0
        POP_32 w8
        POP_32 w6
        POP_32 w5
        STORE_REGISTER w5, w9
        STORE_REGISTER w8, w6
        _emit_lwzu_after:


.globl _emit_lwzx
_emit_lwzx:
          adr x2, _emit_lwzx_start
          adr x3, _emit_lwzx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lwzx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_lwzx_else
        mov w3, 0
        b _emit_lwzx_finally
        _emit_lwzx_else:
        LOAD_REGISTER w1, w3
        _emit_lwzx_finally:
        mov w5, w0
        add w0, w3, w2
        mov w1, 1
        PUSH_32 w0
        PUSH_32 w5
        CALL_HELPER_FUNCTION w1
        mov w8, w0
        POP_32 w5
        POP_32 w0
        STORE_REGISTER w5, w8
        _emit_lwzx_after:


.globl _emit_lwzux
_emit_lwzux:
          adr x2, _emit_lwzux_start
          adr x3, _emit_lwzux_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lwzux_start:
        LOAD_REGISTER w2, w2
        LOAD_REGISTER w1, w3
        add w6, w3, w2
        mov w5, w0
        mov w8, w1
        mov w0, w6
        mov w1, 1
        PUSH_32 w5
        PUSH_32 w6
        PUSH_32 w8
        CALL_HELPER_FUNCTION w1
        mov w9, w0
        POP_32 w8
        POP_32 w6
        POP_32 w5
        STORE_REGISTER w5, w9
        STORE_REGISTER w8, w6
        _emit_lwzux_after:


.globl _emit_stwx
_emit_stwx:
          adr x2, _emit_stwx_start
          adr x3, _emit_stwx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_stwx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_stwx_else
        mov w4, 0
        b _emit_stwx_finaly
        _emit_stwx_else:
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        _emit_stwx_finaly:
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w0, w5
        mov w1, w6
        mov x7, 0
        CALL_HELPER_FUNCTION w7
        _emit_stwx_after:


.globl _emit_stwux
_emit_stwux:
          adr x2, _emit_stwux_start
          adr x3, _emit_stwux_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_stwux_start:
        LOAD_REGISTER w2, w2
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w8, w1
        mov w0, w5
        mov w1, w6
        mov w7, 0
        PUSH_32 w5
        PUSH_32 w8
        CALL_HELPER_FUNCTION w7
        POP_32 w8
        POP_32 w5
        str w5, [GUEST_REGISTER_POINTER, w8, uxtw 2]
        _emit_stwux_after:


.globl _emit_lmw
_emit_lmw:
          adr x2, _emit_lmw_start
          adr x3, _emit_lmw_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lmw_start:
        cbnz w1, lmw_1
        mov w5, 0
        b lmw_2
lmw_1:
        LOAD_REGISTER w1, w5
lmw_2:
        add w6, w5, w2

        mov w5, w0
lmw_3:
        cmp w5, 31
        b.gt _emit_lmw_after

        PUSH_32 w5
        PUSH_32 w6
        mov w0, w6
        mov w2, 1
        CALL_HELPER_FUNCTION w2
        mov w1, w0
        POP_32 w6
        POP_32 w5
        STORE_REGISTER w5, w1
        add w5, w5, 1
        add w6, w6, 4
        b lmw_3

        _emit_lmw_after:


.globl _emit_bcctr
_emit_bcctr:
          adr x2, _emit_bcctr_start
          adr x3, _emit_bcctr_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_bcctr_start:
        ldr  w10, [x5]
        lsr  w10, w10, w1
        and  w10, w10, #1
        ubfx w9, w0, #3, #1
        eor  w10, w10, w9
        eor  w10, w10, #1
        ubfx w11, w0, #4, #1
        orr  w10, w10, w11
        ldr  w9, [x15]
        cbz  w3, bcctr_4
        add  w11, w12, #4
        str  w11, [x14]
bcctr_4:
        cbz  w10, bcctr_3
        bic  w9, w9, #3
        str  w9, [x13]
        ret
bcctr_3:
        add  w9, w12, #4
        str  w9, [x13]
        ret
        _emit_bcctr_after:


.globl _emit_mfsrin
_emit_mfsrin:
          adr x2, _emit_mfsrin_start
          adr x3, _emit_mfsrin_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_mfsrin_start:
        LOAD_REGISTER w2, w2
        lsr w2, w2, #28
        ldr w3, [x1, w2, uxtw 2]
        str w3, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        _emit_mfsrin_after:


.globl _emit_mtsrin
_emit_mtsrin:
          adr x2, _emit_mtsrin_start
          adr x3, _emit_mtsrin_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_mtsrin_start:
        LOAD_REGISTER w2, w2
        lsr w2, w2, #28
        LOAD_REGISTER w0, w4
        str w4, [x1, w2, uxtw 2]
        _emit_mtsrin_after:



.globl _emit_lbzu
_emit_lbzu:
          adr x2, _emit_lbzu_start
          adr x3, _emit_lbzu_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lbzu_start:
        LOAD_REGISTER w1, w3
        add w6, w3, w2
        mov w5, w0
        mov w8, w1
        mov w0, w6
        mov w1, 3
        PUSH_32 w5
        PUSH_32 w6
        PUSH_32 w8
        CALL_HELPER_FUNCTION w1
        mov w9, w0
        POP_32 w8
        POP_32 w6
        POP_32 w5
        STORE_REGISTER w5, w9
        STORE_REGISTER w8, w6
        _emit_lbzu_after:

.globl _emit_stbu
_emit_stbu:
          adr x2, _emit_stbu_start
          adr x3, _emit_stbu_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_stbu_start:

        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        
        _emit_stbu_finaly:
        add w5, w4, w2

        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]

        mov w8, w1

        mov w0, w5
        mov w1, w6
        mov w7, 4

        PUSH_32 w5
        PUSH_32 w8
        CALL_HELPER_FUNCTION w7 // _write( u32 adr, u32 val) // str    w6, [FUNCTION_ARRAY_POINTER, w5, uxtw]
        POP_32 w8
        POP_32 w5

        str w5, [GUEST_REGISTER_POINTER, w8, uxtw 2]
        _emit_stbu_after:



.globl _emit_lhz
_emit_lhz:
          adr x2, _emit_lhz_start
          adr x3, _emit_lhz_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lhz_start:
        cbnz w1, _emit_lhz_else
        mov w3, 0
        b _emit_lhz_finally
        _emit_lhz_else:
        LOAD_REGISTER w1, w3
        _emit_lhz_finally:
        mov w5, w0
        add w0, w3, w2
        mov w1, 6
        PUSH_32 w0
        PUSH_32 w5
        CALL_HELPER_FUNCTION w1
        mov w8, w0
        POP_32 w5
        POP_32 w0
        STORE_REGISTER w5, w8
        _emit_lhz_after:


.globl _emit_lbz
_emit_lbz:
          adr x2, _emit_lbz_start
          adr x3, _emit_lbz_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lbz_start:
        cbnz w1, _emit_lbz_else
        mov w3, 0
        b _emit_lbz_finally
        _emit_lbz_else:
        LOAD_REGISTER w1, w3
        _emit_lbz_finally:
        mov w5, w0
        add w0, w3, w2
        mov w1, 3
        PUSH_32 w0
        PUSH_32 w5
        CALL_HELPER_FUNCTION w1
        mov w8, w0
        POP_32 w5
        POP_32 w0
        STORE_REGISTER w5, w8
        _emit_lbz_after:


.globl _emit_lha
_emit_lha:
          adr x2, _emit_lha_start
          adr x3, _emit_lha_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lha_start:
        cbnz w1, _emit_lha_else
        mov w3, 0
        b _emit_lha_finally
        _emit_lha_else:
        LOAD_REGISTER w1, w3
        _emit_lha_finally:
        mov w5, w0
        add w0, w3, w2
        mov w1, 6
        PUSH_32 w0
        PUSH_32 w5
        CALL_HELPER_FUNCTION w1
        sxth w8, w0
        POP_32 w5
        POP_32 w0
        STORE_REGISTER w5, w8
        _emit_lha_after:


.globl _emit_lhzu
_emit_lhzu:
          adr x2, _emit_lhzu_start
          adr x3, _emit_lhzu_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lhzu_start:
        LOAD_REGISTER w1, w3
        add w6, w3, w2
        mov w5, w0
        mov w8, w1
        mov w0, w6
        mov w1, 6
        PUSH_32 w5
        PUSH_32 w6
        PUSH_32 w8
        CALL_HELPER_FUNCTION w1
        mov w9, w0
        POP_32 w8
        POP_32 w6
        POP_32 w5
        STORE_REGISTER w5, w9
        STORE_REGISTER w8, w6
        _emit_lhzu_after:


.globl _emit_lhau
_emit_lhau:
          adr x2, _emit_lhau_start
          adr x3, _emit_lhau_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lhau_start:
        LOAD_REGISTER w1, w3
        add w6, w3, w2
        mov w5, w0
        mov w8, w1
        mov w0, w6
        mov w1, 6
        PUSH_32 w5
        PUSH_32 w6
        PUSH_32 w8
        CALL_HELPER_FUNCTION w1
        sxth w9, w0
        POP_32 w8
        POP_32 w6
        POP_32 w5
        STORE_REGISTER w5, w9
        STORE_REGISTER w8, w6
        _emit_lhau_after:


.globl _emit_stb
_emit_stb:
          adr x2, _emit_stb_start
          adr x3, _emit_stb_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_stb_start:
        cbnz w1, _emit_stb_else
        mov w4, 0
        b _emit_stb_finaly
        _emit_stb_else:
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        _emit_stb_finaly:
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w0, w5
        and w1, w6, 0xff
        mov x7, 4
        CALL_HELPER_FUNCTION w7
        _emit_stb_after:


.globl _emit_sthu
_emit_sthu:
          adr x2, _emit_sthu_start
          adr x3, _emit_sthu_after
          str x2, [x0]
          str x3, [x1]
          ret          
        _emit_sthu_start:
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w8, w1
        mov w0, w5
        and w1, w6, 0xffff
        mov w7, 5
        PUSH_32 w5
        PUSH_32 w8
        CALL_HELPER_FUNCTION w7
        POP_32 w8
        POP_32 w5
        str w5, [GUEST_REGISTER_POINTER, w8, uxtw 2]
        _emit_sthu_after:


.globl _emit_lbzx
_emit_lbzx:
          adr x2, _emit_lbzx_start
          adr x3, _emit_lbzx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lbzx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_lbzx_else
        mov w3, 0
        b _emit_lbzx_finally
        _emit_lbzx_else:
        LOAD_REGISTER w1, w3
        _emit_lbzx_finally:
        mov w5, w0
        add w0, w3, w2
        mov w1, 3
        PUSH_32 w0
        PUSH_32 w5
        CALL_HELPER_FUNCTION w1
        mov w8, w0
        POP_32 w5
        POP_32 w0
        STORE_REGISTER w5, w8
        _emit_lbzx_after:


.globl _emit_lhzx
_emit_lhzx:
          adr x2, _emit_lhzx_start
          adr x3, _emit_lhzx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lhzx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_lhzx_else
        mov w3, 0
        b _emit_lhzx_finally
        _emit_lhzx_else:
        LOAD_REGISTER w1, w3
        _emit_lhzx_finally:
        mov w5, w0
        add w0, w3, w2
        mov w1, 6
        PUSH_32 w0
        PUSH_32 w5
        CALL_HELPER_FUNCTION w1
        mov w8, w0
        POP_32 w5
        POP_32 w0
        STORE_REGISTER w5, w8
        _emit_lhzx_after:


.globl _emit_lhax
_emit_lhax:
          adr x2, _emit_lhax_start
          adr x3, _emit_lhax_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lhax_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_lhax_else
        mov w3, 0
        b _emit_lhax_finally
        _emit_lhax_else:
        LOAD_REGISTER w1, w3
        _emit_lhax_finally:
        mov w5, w0
        add w0, w3, w2
        mov w1, 6
        PUSH_32 w0
        PUSH_32 w5
        CALL_HELPER_FUNCTION w1
        sxth w8, w0
        POP_32 w5
        POP_32 w0
        STORE_REGISTER w5, w8
        _emit_lhax_after:


.globl _emit_lwbrx
_emit_lwbrx:
          adr x2, _emit_lwbrx_start
          adr x3, _emit_lwbrx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lwbrx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_lwbrx_else
        mov w3, 0
        b _emit_lwbrx_finally
        _emit_lwbrx_else:
        LOAD_REGISTER w1, w3
        _emit_lwbrx_finally:
        mov w5, w0
        add w0, w3, w2
        mov w1, 1
        PUSH_32 w0
        PUSH_32 w5
        CALL_HELPER_FUNCTION w1
        rev w8, w0
        POP_32 w5
        POP_32 w0
        STORE_REGISTER w5, w8
        _emit_lwbrx_after:


.globl _emit_lhbrx
_emit_lhbrx:
          adr x2, _emit_lhbrx_start
          adr x3, _emit_lhbrx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lhbrx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_lhbrx_else
        mov w3, 0
        b _emit_lhbrx_finally
        _emit_lhbrx_else:
        LOAD_REGISTER w1, w3
        _emit_lhbrx_finally:
        mov w5, w0
        add w0, w3, w2
        mov w1, 6
        PUSH_32 w0
        PUSH_32 w5
        CALL_HELPER_FUNCTION w1
        and w0, w0, 0xffff
        rev16 w8, w0
        POP_32 w5
        POP_32 w0
        STORE_REGISTER w5, w8
        _emit_lhbrx_after:


.globl _emit_lbzux
_emit_lbzux:
          adr x2, _emit_lbzux_start
          adr x3, _emit_lbzux_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lbzux_start:
        LOAD_REGISTER w2, w2
        LOAD_REGISTER w1, w3
        add w6, w3, w2
        mov w5, w0
        mov w8, w1
        mov w0, w6
        mov w1, 3
        PUSH_32 w5
        PUSH_32 w6
        PUSH_32 w8
        CALL_HELPER_FUNCTION w1
        mov w9, w0
        POP_32 w8
        POP_32 w6
        POP_32 w5
        STORE_REGISTER w5, w9
        STORE_REGISTER w8, w6
        _emit_lbzux_after:


.globl _emit_lhzux
_emit_lhzux:
          adr x2, _emit_lhzux_start
          adr x3, _emit_lhzux_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lhzux_start:
        LOAD_REGISTER w2, w2
        LOAD_REGISTER w1, w3
        add w6, w3, w2
        mov w5, w0
        mov w8, w1
        mov w0, w6
        mov w1, 6
        PUSH_32 w5
        PUSH_32 w6
        PUSH_32 w8
        CALL_HELPER_FUNCTION w1
        mov w9, w0
        POP_32 w8
        POP_32 w6
        POP_32 w5
        STORE_REGISTER w5, w9
        STORE_REGISTER w8, w6
        _emit_lhzux_after:


.globl _emit_lhaux
_emit_lhaux:
          adr x2, _emit_lhaux_start
          adr x3, _emit_lhaux_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lhaux_start:
        LOAD_REGISTER w2, w2
        LOAD_REGISTER w1, w3
        add w6, w3, w2
        mov w5, w0
        mov w8, w1
        mov w0, w6
        mov w1, 6
        PUSH_32 w5
        PUSH_32 w6
        PUSH_32 w8
        CALL_HELPER_FUNCTION w1
        sxth w9, w0
        POP_32 w8
        POP_32 w6
        POP_32 w5
        STORE_REGISTER w5, w9
        STORE_REGISTER w8, w6
        _emit_lhaux_after:


.globl _emit_stbx
_emit_stbx:
          adr x2, _emit_stbx_start
          adr x3, _emit_stbx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_stbx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_stbx_else
        mov w4, 0
        b _emit_stbx_finaly
        _emit_stbx_else:
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        _emit_stbx_finaly:
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w0, w5
        and w1, w6, 0xff
        mov x7, 4
        CALL_HELPER_FUNCTION w7
        _emit_stbx_after:


.globl _emit_sthx
_emit_sthx:
          adr x2, _emit_sthx_start
          adr x3, _emit_sthx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_sthx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_sthx_else
        mov w4, 0
        b _emit_sthx_finaly
        _emit_sthx_else:
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        _emit_sthx_finaly:
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w0, w5
        and w1, w6, 0xffff
        mov x7, 5
        CALL_HELPER_FUNCTION w7
        _emit_sthx_after:


.globl _emit_stwbrx
_emit_stwbrx:
          adr x2, _emit_stwbrx_start
          adr x3, _emit_stwbrx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_stwbrx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_stwbrx_else
        mov w4, 0
        b _emit_stwbrx_finaly
        _emit_stwbrx_else:
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        _emit_stwbrx_finaly:
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w0, w5
        rev w1, w6
        mov x7, 0
        CALL_HELPER_FUNCTION w7
        _emit_stwbrx_after:


.globl _emit_sthbrx
_emit_sthbrx:
          adr x2, _emit_sthbrx_start
          adr x3, _emit_sthbrx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_sthbrx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_sthbrx_else
        mov w4, 0
        b _emit_sthbrx_finaly
        _emit_sthbrx_else:
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        _emit_sthbrx_finaly:
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w0, w5
        and w6, w6, 0xffff
        rev16 w1, w6
        mov x7, 5
        CALL_HELPER_FUNCTION w7
        _emit_sthbrx_after:


.globl _emit_stbux
_emit_stbux:
          adr x2, _emit_stbux_start
          adr x3, _emit_stbux_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_stbux_start:
        LOAD_REGISTER w2, w2
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w8, w1
        mov w0, w5
        and w1, w6, 0xff
        mov w7, 4
        PUSH_32 w5
        PUSH_32 w8
        CALL_HELPER_FUNCTION w7
        POP_32 w8
        POP_32 w5
        str w5, [GUEST_REGISTER_POINTER, w8, uxtw 2]
        _emit_stbux_after:


.globl _emit_sthux
_emit_sthux:
          adr x2, _emit_sthux_start
          adr x3, _emit_sthux_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_sthux_start:
        LOAD_REGISTER w2, w2
        ldr w4, [GUEST_REGISTER_POINTER, w1, uxtw 2]
        add w5, w4, w2
        ldr w6, [GUEST_REGISTER_POINTER, w0, uxtw 2]
        mov w8, w1
        mov w0, w5
        and w1, w6, 0xffff
        mov w7, 5
        PUSH_32 w5
        PUSH_32 w8
        CALL_HELPER_FUNCTION w7
        POP_32 w8
        POP_32 w5
        str w5, [GUEST_REGISTER_POINTER, w8, uxtw 2]
        _emit_sthux_after:


.globl _emit_lswi
_emit_lswi:
          adr x2, _emit_lswi_start
          adr x3, _emit_lswi_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lswi_start:
        cbnz w1, _emit_lswi_else
        mov w5, 0
        b _emit_lswi_finally
        _emit_lswi_else:
        LOAD_REGISTER w1, w5
        _emit_lswi_finally:
        mov w9, w2
        sub w10, w0, 1
        mov w11, 0
        _emit_lswi_loop:
        cbz w9, _emit_lswi_after
        cbnz w11, _emit_lswi_read
        add w10, w10, 1
        and w10, w10, 31
        STORE_REGISTER w10, wzr
        _emit_lswi_read:
        PUSH_32 w5
        PUSH_32 w9
        PUSH_32 w10
        PUSH_32 w11
        mov w0, w5
        mov w1, 3
        CALL_HELPER_FUNCTION w1
        mov w12, w0
        POP_32 w11
        POP_32 w10
        POP_32 w9
        POP_32 w5
        mov w13, 24
        sub w13, w13, w11
        lsl w12, w12, w13
        LOAD_REGISTER w10, w14
        orr w14, w14, w12
        STORE_REGISTER w10, w14
        add w11, w11, 8
        and w11, w11, 31
        add w5, w5, 1
        sub w9, w9, 1
        b _emit_lswi_loop
        _emit_lswi_after:


.globl _emit_lswx
_emit_lswx:
          adr x2, _emit_lswx_start
          adr x3, _emit_lswx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_lswx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_lswx_else
        mov w5, 0
        b _emit_lswx_finally
        _emit_lswx_else:
        LOAD_REGISTER w1, w5
        _emit_lswx_finally:
        add w5, w5, w2
        ldr w9, [x3]
        and w9, w9, 0x7f
        sub w10, w0, 1
        mov w11, 0
        _emit_lswx_loop:
        cbz w9, _emit_lswx_after
        cbnz w11, _emit_lswx_read
        add w10, w10, 1
        and w10, w10, 31
        STORE_REGISTER w10, wzr
        _emit_lswx_read:
        PUSH_32 w5
        PUSH_32 w9
        PUSH_32 w10
        PUSH_32 w11
        mov w0, w5
        mov w1, 3
        CALL_HELPER_FUNCTION w1
        mov w12, w0
        POP_32 w11
        POP_32 w10
        POP_32 w9
        POP_32 w5
        mov w13, 24
        sub w13, w13, w11
        lsl w12, w12, w13
        LOAD_REGISTER w10, w14
        orr w14, w14, w12
        STORE_REGISTER w10, w14
        add w11, w11, 8
        and w11, w11, 31
        add w5, w5, 1
        sub w9, w9, 1
        b _emit_lswx_loop
        _emit_lswx_after:


.globl _emit_stswi
_emit_stswi:
          adr x2, _emit_stswi_start
          adr x3, _emit_stswi_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_stswi_start:
        cbnz w1, _emit_stswi_else
        mov w5, 0
        b _emit_stswi_finally
        _emit_stswi_else:
        LOAD_REGISTER w1, w5
        _emit_stswi_finally:
        mov w9, w2
        sub w10, w0, 1
        mov w11, 0
        _emit_stswi_loop:
        cbz w9, _emit_stswi_after
        cbnz w11, _emit_stswi_write
        add w10, w10, 1
        and w10, w10, 31
        _emit_stswi_write:
        LOAD_REGISTER w10, w12
        mov w13, 24
        sub w13, w13, w11
        lsr w12, w12, w13
        and w12, w12, 0xff
        PUSH_32 w5
        PUSH_32 w9
        PUSH_32 w10
        PUSH_32 w11
        mov w0, w5
        mov w1, w12
        mov w7, 4
        CALL_HELPER_FUNCTION w7
        POP_32 w11
        POP_32 w10
        POP_32 w9
        POP_32 w5
        add w11, w11, 8
        and w11, w11, 31
        add w5, w5, 1
        sub w9, w9, 1
        b _emit_stswi_loop
        _emit_stswi_after:


.globl _emit_stswx
_emit_stswx:
          adr x2, _emit_stswx_start
          adr x3, _emit_stswx_after
          str x2, [x0]
          str x3, [x1]
          ret  
        _emit_stswx_start:
        LOAD_REGISTER w2, w2
        cbnz w1, _emit_stswx_else
        mov w5, 0
        b _emit_stswx_finally
        _emit_stswx_else:
        LOAD_REGISTER w1, w5
        _emit_stswx_finally:
        add w5, w5, w2
        ldr w9, [x3]
        and w9, w9, 0x7f
        sub w10, w0, 1
        mov w11, 0
        _emit_stswx_loop:
        cbz w9, _emit_stswx_after
        cbnz w11, _emit_stswx_write
        add w10, w10, 1
        and w10, w10, 31
        _emit_stswx_write:
        LOAD_REGISTER w10, w12
        mov w13, 24
        sub w13, w13, w11
        lsr w12, w12, w13
        and w12, w12, 0xff
        PUSH_32 w5
        PUSH_32 w9
        PUSH_32 w10
        PUSH_32 w11
        mov w0, w5
        mov w1, w12
        mov w7, 4
        CALL_HELPER_FUNCTION w7
        POP_32 w11
        POP_32 w10
        POP_32 w9
        POP_32 w5
        add w11, w11, 8
        and w11, w11, 31
        add w5, w5, 1
        sub w9, w9, 1
        b _emit_stswx_loop
        _emit_stswx_after:
