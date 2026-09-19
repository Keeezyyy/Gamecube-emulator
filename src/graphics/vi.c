#include "vi.h"
#include "bus/interfaces/pi.h"
#include "core/config/config.h"
#include "scheduler/scheduler.h"
#include <_time.h>
#include <assert.h>
#include <stdbool.h>
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

static void vi_clock(CPU *cpu)
{

    // vi interrupt
    u32 hlw = dpr.HTR0 & 0x1FF;

    u32 lines = 263; // TODO: aus VTR/VTO/VTE ableiten (NTSC vs PAL)
    for (u32 vP = 1; vP <= lines; vP++)
        for (u32 hP = 1; hP <= 2 * hlw; hP++)
            for (int i = 0; i < 4; i++) {
                u32 di = (&dpr.DI0)[i];
                if ((di >> 28 & 1) && (di & 0x3FF) == hP && ((di >> 16) & 0x3FF) == vP) {
                    (&dpr.DI0)[i] |= BIT(31);
                    pi_activate_external_interrupt(cpu, INTERRUPT_SOURCE_VI);
                }
            }
}
static SchedulerEvent e = {.active = false, .callback = &vi_clock, .clock_speed = 27000000};

void vi_write(CPU *cpu, u32 adr, u32 val, u32 size)
{

    assert(size == 2 || size == 4);

    if (adr == 0xCC002002 && (val >> 1) & 1) {
        // rst interrupts
        for (int i = 0; i < 4; i++) {
            (&dpr.DI0)[i] &= ~BIT(31);
        }
    }

    if (adr == 0xCC00206C) {
        // set vi clock speed;

        if (val & 1) {
            e.clock_speed = 54000000;
        } else {
            e.clock_speed = 27000000;
        }

        scheduler_edit_event(SCHEDULER_EVENT_VI, e);
        return;
    } else if (adr == 0xCC002004 || adr == 0xCC002008) {
        e.active = true;
        scheduler_edit_event(SCHEDULER_EVENT_VI, e);
    }

    volatile u8 *p = (volatile u8 *)&dpr + vi_offset(adr, size);
    if (size == 2) {
        *(volatile u16 *)p = (u16)val;
    } else {
        *(volatile u32 *)p = val;
    }

    for (int i = 0; i < 4; i++) {
        if (((&dpr.DI0)[i] >> 31))
            return;
        pi_deactivate_external_interrupt(cpu, INTERRUPT_SOURCE_VI);
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
void vi_init(void)
{
    scheduler_add_event_to_buffer(SCHEDULER_EVENT_VI, e);
}
