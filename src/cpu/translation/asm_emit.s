
.globl _emit_addi
_emit_addi:
          //void emit_addi(*void start, *void end)  // imm val is on the stack 
          adr x22, _emit_addi_start
          adr x23, _emit_addi_after
          STR x22, [x0]
          STR x23, [x1]

          //x0 regA_ptr
          //x1 regb_ptr
          //x2 imm
          ret
        _emit_addi_start:
          ldr w3, [x0] // w19 A reg
          LDRSH w4, [x2] // w20 imm

          cbnz w3, _emit_addi_start_else
          STR w4, [x1]// store w20 in B reg
          B _emit_addi_after
        _emit_addi_start_else:
          ldr w3, [x0]
          add w4, w3, w2
          str w4, [x1]
        _emit_addi_after:


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
          ldr w3, [x0]
          orr w4, w2, w0
          str w4, [x1]
        _emit_ori_after:
