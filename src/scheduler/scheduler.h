#pragma once

#include "cpu/cpu_types.h"
#include <stdbool.h>
#define CPU_CLOCK_SPEED 485000000u

extern u64 global_cycle_counter;

enum SchedulerEventTypes {
    SCHEDULER_EVENT_VI,
    SCHEDULER_EVENT_AI,
    SCHEDULER_EVENT_COUNT,

};

typedef struct {
    void (*callback)(CPU *cpu);
    u64 clock_speed;
    bool active;
} SchedulerEvent;

void report_cycle_count(CPU *cpu, u32 cycle_count);

void scheduler_add_event_to_buffer(enum SchedulerEventTypes, SchedulerEvent);

void scheduler_edit_event(enum SchedulerEventTypes t, SchedulerEvent e);
