#include "cp.h"
#include "bus/interfaces/pi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static GXFifoRegs cp_regs;

void cp_write(CPU *cpu, u32 adr, u32 val, u32 size)
{
    printf("cp write : adr : 0x%08x, val : 0x%08x\n", adr, val);

    if (adr == 0xCC000004) {
        if (val & 1) {
            cp_regs.SR &= ~0x1;
        }
        if ((val >> 1) & 1) {
            cp_regs.SR &= ~0x2;
        }
        if ((val >> 2) & 1) {
            // assert(!"cp write clear metrics not implemented\n");
        }
    }

    assert(size == 2);

    volatile u8 *p = (volatile u8 *)&cp_regs;
    if (size == 2) {
        *(volatile u16 *)(p + (adr - 0xCC000000)) = (u16)val;
    } else {
        *(volatile u32 *)(p + (adr - 0xCC000000)) = (u32)val;
    }

    cp_check_state(cpu);
}
u64 cp_read(CPU *cpu, u32 adr, u32 size)
{
    printf("read : 0x%08x\n", adr);
    volatile u8 *p = (volatile u8 *)&cp_regs;
    if (size == 2) {
        return *(volatile u16 *)(p + (adr - 0xCC000000));
    } else {

        return *(volatile u32 *)(p + (adr - 0xCC000000));
    }
}

void cp_check_state(CPU *cpu)
{
    if (cp_regs.RW_DISTANCE > cp_regs.HI_WATERMARK) {
        cp_regs.SR |= 1;
    }
    if (cp_regs.RW_DISTANCE < cp_regs.LO_WATERMARK) {
        cp_regs.SR |= 2;
    }

    pi_update_interrupts(cpu);
}
u32 cp_get_cr_reg(void)
{
    return cp_regs.CR;
}

u32 cp_get_sr_reg(void)
{
    return cp_regs.SR;
}

void cp_recieve_gather_pipe(CPU *cpu)
{
    // read the data stream ...

    cp_regs.WRITE_POINTER += 32;
    if (cp_regs.WRITE_POINTER > cp_regs.FIFO_END)
        cp_regs.WRITE_POINTER = cp_regs.FIFO_BASE;

    cp_regs.READ_POINTER = cp_regs.WRITE_POINTER;
    cp_regs.RW_DISTANCE = 0;

    cp_check_state(cpu);
}

GXFifoRegs *get_cp_regs(void)
{
    return &cp_regs;
}
