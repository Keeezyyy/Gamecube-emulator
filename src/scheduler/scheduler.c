#include "scheduler.h"
#include "core/config/config.h"
#include <math.h>

u64 global_cycle_counter;
static SchedulerEvent event_buffer[SCHEDULER_EVENT_COUNT];

void report_cycle_count(CPU *cpu, u32 cycle_count)
{
    u64 next = global_cycle_counter + cycle_count;
    for (int j = 0; j < SCHEDULER_EVENT_COUNT; j++) {
        if (!event_buffer[j].active || event_buffer[j].callback == NULL_PTR)
            continue;

        u64 event_every_x_host_cycles = CPU_CLOCK_SPEED / event_buffer[j].clock_speed;

        if ((floor((double)global_cycle_counter / (double)event_every_x_host_cycles) !=
             floor((double)next / (double)event_every_x_host_cycles))) {

            event_buffer[j].callback(cpu);
        }
    }
    u64 elapsed_ticks = next / 12 - global_cycle_counter / 12;
    global_cycle_counter = next;

    u64 time_base = ((u64)cpu->special_purpose_registers.buf[269] << 32 |
                     cpu->special_purpose_registers.buf[268]) +
                    elapsed_ticks;
    cpu->special_purpose_registers.buf[268] = time_base & U32_MAX;
    cpu->special_purpose_registers.buf[269] = time_base >> 32;
}

void scheduler_add_event_to_buffer(enum SchedulerEventTypes t, SchedulerEvent e)
{
    event_buffer[t] = e;
}
void scheduler_edit_event(enum SchedulerEventTypes t, SchedulerEvent e)
{
    event_buffer[t] = e;
}
