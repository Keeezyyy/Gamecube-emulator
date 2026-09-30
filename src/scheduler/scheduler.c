#include "scheduler.h"
#include "core/config/config.h"
#include "cpu/cpu.h"
#include <math.h>
#include <stdbool.h>

static CPU *cpu_ptr;

static SchedulerEvent event_buffer[SCHEDULER_EVENT_COUNT];
static SchedulerOneTimeEvent one_time_event_buffer[SCHEDULER_ONE_TIME_EVENT_COUNT];
static u64 event_cycle_acc[SCHEDULER_EVENT_COUNT];
static u64 event_last_cycle[SCHEDULER_EVENT_COUNT];

static u64 next_deadline = 0;

static inline void next_deadline_add(u64 cycle)
{
    next_deadline = next_deadline <= cycle ? next_deadline : cycle;
}

static inline void next_deadline_add_event(enum SchedulerEventTypes j)
{
    u64 event_every_x_host_cycles = CPU_CLOCK_SPEED / event_buffer[j].clock_speed;
    next_deadline_add(event_last_cycle[j] + event_every_x_host_cycles - event_cycle_acc[j]);
}

static void run_event(CPU *cpu)
{
    next_deadline = U64_MAX;

    for (int j = 0; j < SCHEDULER_EVENT_COUNT; j++) {
        if (!event_buffer[j].active || event_buffer[j].callback == NULL_PTR)
            continue;

        u64 event_every_x_host_cycles = CPU_CLOCK_SPEED / event_buffer[j].clock_speed;

        event_cycle_acc[j] += cpu->cpu_cycles - event_last_cycle[j];
        event_last_cycle[j] = cpu->cpu_cycles;
        while (event_cycle_acc[j] >= event_every_x_host_cycles) {
            event_cycle_acc[j] -= event_every_x_host_cycles;
            event_buffer[j].callback(cpu);
        }

        if (event_buffer[j].active)
            next_deadline_add_event((enum SchedulerEventTypes)j);
    }

    for (int j = 0; j < SCHEDULER_ONE_TIME_EVENT_COUNT; j++) {
        if (!one_time_event_buffer[j].active || one_time_event_buffer[j].callback == NULL_PTR)
            continue;
        if (cpu->cpu_cycles >= one_time_event_buffer[j].activate_on_cycle) {
            one_time_event_buffer[j].active = false;
            one_time_event_buffer[j].callback(cpu);
        }

        if (one_time_event_buffer[j].active)
            next_deadline_add(one_time_event_buffer[j].activate_on_cycle);
    }
}

void report_cycle_count(u32 cycle_count)
{
    u64 next = cpu_ptr->cpu_cycles + cycle_count;

    update_decrementor(cpu_ptr, cpu_ptr->cpu_cycles, next);

    u64 elapsed_ticks = next / 12 - cpu_ptr->cpu_cycles / 12;
    cpu_ptr->cpu_cycles = next;

    u64 time_base = ((u64)cpu_ptr->special_purpose_registers.buf[269] << 32 |
                     cpu_ptr->special_purpose_registers.buf[268]) +
                    elapsed_ticks;
    cpu_ptr->special_purpose_registers.buf[268] = time_base & U32_MAX;
    cpu_ptr->special_purpose_registers.buf[269] = time_base >> 32;

    if (next >= next_deadline)
        run_event(cpu_ptr);
}

void init_scheduler(CPU *cpu)
{
    cpu_ptr = cpu;
}

void scheduler_add_event_to_buffer(enum SchedulerEventTypes t, SchedulerEvent e)
{
    scheduler_edit_event(t, e);
}
void scheduler_edit_event(enum SchedulerEventTypes t, SchedulerEvent e)
{
    if (e.active && !event_buffer[t].active) {
        event_cycle_acc[t] = 0;
        event_last_cycle[t] = cpu_ptr->cpu_cycles;
    }

    event_buffer[t] = e;

    if (e.active)
        next_deadline_add_event(t);
}

void scheduler_add_one_time_event(enum SchedulerOneTimeEventTypes t, SchedulerOneTimeEvent e)
{
    one_time_event_buffer[t] = e;

    if (e.active)
        next_deadline_add(e.activate_on_cycle);
}

void scheduler_activate_one_time_event(enum SchedulerOneTimeEventTypes t, u64 cycles_from_now)
{

    one_time_event_buffer[t].active = true;
    one_time_event_buffer[t].activate_on_cycle = cpu_ptr->cpu_cycles + cycles_from_now;

    next_deadline_add(one_time_event_buffer[t].activate_on_cycle);
}
void scheduler_edit_one_time_event(enum SchedulerOneTimeEventTypes t, SchedulerOneTimeEvent e)
{
    one_time_event_buffer[t] = e;

    if (e.active)
        next_deadline_add(e.activate_on_cycle);
}
