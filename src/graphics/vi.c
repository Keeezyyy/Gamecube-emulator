#include "vi.h"
#include "bus/interfaces/pi.h"
#include "bus/interfaces/si.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
#include "io/io.h"
#include "scheduler/scheduler.h"
#include <_time.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <SDL3/SDL.h>

static DisplayRegisters dpr;

u32 vi_get_display_interrupt(u8 index)
{
    return (&dpr.DI0)[index];
}

static u32 vi_offset(u32 adr, u32 size)
{
    u32 off = adr - 0xCC002000;
    if (size == 2 && off >= 0x30 && off < 0x40) {
        off ^= 2;
    }
    return off;
}

static SchedulerEvent e = {.active = false, .callback = 0x0, .clock_speed = 27000000};

static u64 full_vblank_in_cycles = 0;

static u64 get_full_vblank_in_cycles(void)
{
    if (full_vblank_in_cycles == 0) {

        const u32 hlw = dpr.HTR0 & 0x1FF;
        const u32 lines = ((dpr.DCR >> 8) & 3) ? 313 : 263;
        const double clock_ratio = (double)CPU_CLOCK_SPEED / (double)e.clock_speed;

        full_vblank_in_cycles = (u64)(hlw * 2 * lines * (clock_ratio));
    }
    return full_vblank_in_cycles;
}

static u64 frame_start_cycle = 0;

static void vi_scheduler_callback(CPU *cpu, const u8 num)
{

    if (num < 4) {
        (&dpr.DI0)[num] |= BIT(31);
        pi_update_interrupts(cpu);
    } else {
        si_vblank_trigger();
        trigger_frame();
        cp_wait_idle();
    }
    scheduler_activate_one_time_event(SCHEDULER_ONE_TIME_EVENT_VI_DI0 + num,
                                      get_full_vblank_in_cycles());
}

static void di0_callback(CPU *cpu)
{
    vi_scheduler_callback(cpu, 0);
}
static void di1_callback(CPU *cpu)
{
    vi_scheduler_callback(cpu, 1);
}
static void di2_callback(CPU *cpu)
{
    vi_scheduler_callback(cpu, 2);
}
static void di3_callback(CPU *cpu)
{
    vi_scheduler_callback(cpu, 3);
}
static void vblank_callback(CPU *cpu)
{
    vi_scheduler_callback(cpu, 4);
    frame_start_cycle = cpu->cpu_cycles;
}

static void (*callback_funcs[4])(CPU *cpu) = {&di0_callback, &di1_callback, &di2_callback,
                                              &di3_callback};

// von x, y von 0 ausgehen
// TODO: use actual x and y vals
static void update_scheduler_events(CPU *cpu)
{

    const u32 hlw = dpr.HTR0 & 0x1FF;
    const u32 lines = ((dpr.DCR >> 8) & 3) ? 313 : 263;
    const double clock_ratio = (double)CPU_CLOCK_SPEED / (double)e.clock_speed;

    full_vblank_in_cycles = (u64)(hlw * 2 * lines * (clock_ratio));

    for (int i = 0; i < 4; i++) {
        const u32 di = (&dpr.DI0)[i];
        if (!((di >> 28) & 1)) {
            scheduler_edit_one_time_event(SCHEDULER_ONE_TIME_EVENT_VI_DI0 + i,
                                          (SchedulerOneTimeEvent){.active = false});
            return;
        }

        const u16 hct = di & 0x7FF;
        const u16 vct = (di >> 16) & 0x7FF;
        u64 target = frame_start_cycle + (u64)(((vct - 1) * 2 * hlw + hct) * clock_ratio);
        while (target <= cpu->cpu_cycles)
            target += full_vblank_in_cycles;

        scheduler_add_one_time_event(SCHEDULER_ONE_TIME_EVENT_VI_DI0 + i,
                                     (SchedulerOneTimeEvent){.active = true,
                                                             .activate_on_cycle = target,
                                                             .callback = callback_funcs[i]});
    }

    SchedulerOneTimeEvent one_time_event = (SchedulerOneTimeEvent){
        .active = true, .activate_on_cycle = full_vblank_in_cycles, .callback = vblank_callback};

    scheduler_add_one_time_event(SCHEDULER_ONE_TIME_EVENT_VI_V_BLANK, one_time_event);
}

void vi_write(CPU *cpu, u32 adr, u32 val, u32 size)
{

    assert(size == 2 || size == 4);

    if (adr == 0xCC002002 && (val >> 1) & 1) {
        // rst interrupts
        for (int i = 0; i < 4; i++) {
            (&dpr.DI0)[i] = 0;
        }
    }

    if (adr == 0xCC00206C) {
        // set vi clock speed;

        if (val & 1) {
            e.clock_speed = 54000000;
        } else {
            e.clock_speed = 27000000;
        }

        return;
    } else if (adr == 0xCC002004 || adr == 0xCC002008) {
    }

    volatile u8 *p = (volatile u8 *)&dpr + vi_offset(adr, size);
    if (size == 2) {
        *(volatile u16 *)p = (u16)val;
    } else {
        *(volatile u32 *)p = val;
    }

    pi_update_interrupts(cpu);
    update_scheduler_events(cpu);
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
}
