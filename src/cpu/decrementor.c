#include "core/config/config.h"
#include "cpu.h"
#include "cpu/cpu_types.h"
#include "scheduler/scheduler.h"
#include <stdlib.h>

static u32 last_val = 0;
static u64 last_cycle = 0;
static SchedulerOneTimeEvent e;

static u32 _re_calculate(CPU *cpu)
{
    const u32 new_val = last_val - (u32)((cpu->cpu_cycles - last_cycle) / 12);
    last_val = new_val;
    last_cycle = cpu->cpu_cycles;
    return new_val;
}

static void _decrementor_scheduler_callback(CPU *cpu)
{
    cpu->special_purpose_registers.buf[22] = _re_calculate(cpu);
    cpu->trigger_internal_interrupt(cpu, INTERNAL_INTERRUPT_TYPE_DECREMENT, true);
}

u32 decrementor_read(CPU *cpu)
{
    u32 new_val = _re_calculate(cpu);
    cpu->special_purpose_registers.buf[22] = new_val;
    return new_val;
}

void update_decrementor(CPU *cpu)
{
    u32 new_val = cpu->special_purpose_registers.buf[22];

    u32 cycles_till_bit_flip = new_val % ((BIT(31)) + 1);

    scheduler_activate_one_time_event(SCHEDULER_ONE_TIME_EVENT_DECREMENTOR_TRIGGER,
                                      cycles_till_bit_flip / 12);
    last_val = new_val;
    last_cycle = cpu->cpu_cycles;
}
void decrementor_init(void)
{
    e.activate_on_cycle = 10;
    e.active = true;
    e.callback = &_decrementor_scheduler_callback;

    scheduler_add_one_time_event(SCHEDULER_ONE_TIME_EVENT_DECREMENTOR_TRIGGER, e);
}
