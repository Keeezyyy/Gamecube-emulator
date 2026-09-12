
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
          ldr w19, [x0] // w19 A reg
          LDRSH W20, [x2] // w20 imm

          cbnz w19, _emit_addi_start_else
          STR w20, [x1]// store w20 in B reg
          B _emit_addi_after
        _emit_addi_start_else:
          add w20, w20, w19
          STR w20, [x1]
        _emit_addi_after:


.globl _emit_ori
_emit_ori:
          //void emit_ori(*void start, *void end)  // imm val is on the stack 
          adr x22, _emit_ori_start
          adr x23, _emit_ori_after
          STR x22, [x0]
          STR x23, [x1]

          //x0 regA_ptr
          //x1 regS_ptr
          //x2 imm
          ret
        _emit_ori_start:
          mov w19, 0
          ldr w19, [x2]
          orr w20, w19, w1
          str w20, [x0]
        _emit_ori_after:
