#pragma once

// settings :
#define STATIC_CODE_BLOCK_BUFFER 512
#define MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK 1024
#define GUEST_REGISTER_POINTER 16

// opcodes OPC_...
#define OPC_STMW 47
#define OPC_LFD 50

#define OPC_BX 18

#define OPC_ADDIS 15
#define OPC_ADDI 14
#define OPC_ORI 24
#define OPC_BCLR 19
#define OPC_BCLR_EXT 16

#define OPC_MFMSR 31
#define OPC_MFMSR_EXT 83

#define OPC_MTMSR 31
#define OPC_MTMSR_EXT 146

#define OPC_MFSPR_EXT 339
