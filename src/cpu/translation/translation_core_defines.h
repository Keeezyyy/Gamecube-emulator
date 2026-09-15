#pragma once

// settings :
#define STATIC_CODE_BLOCK_BUFFER 512
#define MAX_GUEST_INSTRUCTIONS_PER_TRANSLATION_BLOCK 1024
#define GUEST_REGISTER_POINTER 19
#define GUEST_HELPER_FUNCTIONS_POINTER 20

// opcodes OPC_...
#define OPC_STMW 47
#define OPC_LFD 50

#define OPC_CMPLI 10
#define OPC_ADDI 14
#define OPC_ADDIS 15
#define OPC_BCLR_EXT 16
#define OPC_ISYNC_EXT 150
#define OPC_CRXOR_EXT 193
#define OPC_BCX 16
#define OPC_BX 18
#define OPC_BCLR 19
#define OPC_RLWINM 21
#define OPC_ORI 24
#define OPC_ORIS 25
#define OPC_MFMSR 31
#define OPC_MFMSR_EXT 83
#define OPC_MTMSR 31
#define OPC_MTMSR_EXT 146
#define OPC_MFSPR_EXT 339
#define OPC_MTSPR_EXT 467
#define OPC_SYNC_EXT 598
#define OPC_LWZ 32
#define OPC_STW 36
#define OPC_STWU 37
#define OPC_STFD 54

#define _CAT(a, b) a##b
#define CAT(a, b) _CAT(a, b)
