#include "vi.h"
#include "bus/interfaces/pi.h"
#include "core/config/config.h"
#include <_time.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

static DisplayRegisters dpr;

static u32 vi_offset(u32 adr, u32 size)
{
    u32 off = adr - 0xCC002000;
    if (size == 2 && off >= 0x30 && off < 0x40) {
        off ^= 2;
    }
    return off;
}

void vi_write(CPU *cpu, u32 adr, u32 val, u32 size)
{

    assert(size == 2 || size == 4);
    volatile u8 *p = (volatile u8 *)&dpr + vi_offset(adr, size);
    if (size == 2) {
        *(volatile u16 *)p = (u16)val;
    } else {
        *(volatile u32 *)p = val;
    }
}

u32 vi_read(CPU *cpu, u32 adr, u32 size)
{
    if (adr == 0xCC002002) {
        return dpr.DCR & ~(2);
    }

    volatile u8 *p = (volatile u8 *)&dpr + vi_offset(adr, size);
    if (size == 2) {
        return *(volatile u16 *)p;
    }
    return *(volatile u32 *)p;
}

void vi_main_loop(CPU *cpu)
{
    const struct timespec frame = {.tv_sec = 0, .tv_nsec = 1666666};

    while (1) {
        // vi interrupt

        if ((dpr.DI0 >> 28 & 1) || (dpr.DI1 >> 28 & 1) || (dpr.DI2 >> 28 & 1) ||
            (dpr.DI3 >> 28 & 1)) { // interrupt mask for at least 1 is active
            pi_activate_external_interrupt(cpu, INTERRUPT_SOURCE_VI);

            // TODO: implement acutal emulation of vi timings
            //  for now set every di reg interrupt

            dpr.DI0 |= BIT(31);
            dpr.DI1 |= BIT(31);
            dpr.DI2 |= BIT(31);
            dpr.DI3 |= BIT(31);
        }
        nanosleep(&frame, NULL);
    }
}
