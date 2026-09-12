
.globl _emit_addi
_emit_addi:
          //void emit_addi(*void start, *void end)  // imm val is on the stack 
          adr x22, _emit_addi_start
          adr x23, _emit_addi_after
          STR x22, [x0]
          STR x23, [x1]

  //x16 register pointer
          //w0 regA_num
          //w1 regD_num
          //w2 imm
          ret
        _emit_addi_start:
          ldr w3, [x16, w0, uxtw 2] // w19 A reg

          cbnz w3, _emit_addi_start_else
          STR w2, [x16, w1, uxtw 2]
          B _emit_addi_after
        _emit_addi_start_else:
          add w4, w3, w2
          STR w4, [x16, w1, uxtw 2]
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
          ldr w3, [x16, w0, uxtw 2]
          orr w4, w2, w3
          str w4, [x16, w1, uxtw 2]
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
          ldr w2, [x16, w1, uxtw 2]
          str w2, [x0]
        _emit_mtmsr_after:


// w0 -> rD_num
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

          str w3, [x16, w0, uxtw 2]
        _emit_mfspr_after:

