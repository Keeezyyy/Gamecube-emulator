#pragma once

#include "cpu/cpu_types.h"

typedef enum {
    MI_ACCESS_READ,
    MI_ACCESS_WRITE,
} MiMemAccessType;

typedef struct {
    uint16_t region0_first; // +0x00
    uint16_t region0_last;  // +0x02

    uint16_t region1_first; // +0x04
    uint16_t region1_last;  // +0x06

    uint16_t region2_first; // +0x08
    uint16_t region2_last;  // +0x0A

    uint16_t region3_first; // +0x0C
    uint16_t region3_last;  // +0x0E

    uint16_t prot_type; // +0x10  2 Bit pro Region

    uint8_t _reserved0[0x0A]; // +0x12..+0x1B

    uint16_t irqmask; // +0x1C
    uint16_t irqflag; // +0x1E  W1C

    uint16_t unknown1; // +0x20

    uint16_t prot_addr_hi; // +0x22
    uint16_t prot_addr_lo; // +0x24

    uint8_t _reserved1[0x0A]; // +0x26..+0x2F

    uint8_t _reserved2[0x02]; // +0x30..+0x31

    uint16_t timer0_hi; // +0x32
    uint16_t timer0_lo; // +0x34
    uint16_t timer1_hi; // +0x36
    uint16_t timer1_lo; // +0x38
    uint16_t timer2_hi; // +0x3A
    uint16_t timer2_lo; // +0x3C
    uint16_t timer3_hi; // +0x3E
    uint16_t timer3_lo; // +0x40
    uint16_t timer4_hi; // +0x42
    uint16_t timer4_lo; // +0x44
    uint16_t timer5_hi; // +0x46
    uint16_t timer5_lo; // +0x48
    uint16_t timer6_hi; // +0x4A
    uint16_t timer6_lo; // +0x4C
    uint16_t timer7_hi; // +0x4E
    uint16_t timer7_lo; // +0x50
    uint16_t timer8_hi; // +0x52
    uint16_t timer8_lo; // +0x54
    uint16_t timer9_hi; // +0x56
    uint16_t timer9_lo; // +0x58

    uint16_t unknown2; // +0x5A
} MemoryInterfaceRegisters;

void mi_write(CPU *cpu, u32 adr, u64 val, u32 size);
u32 mi_read(CPU *cpu, u32 adr, u32 size);

void mi_report_mem_access(CPU *cpu, u32 adr, MiMemAccessType access_type);
