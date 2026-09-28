#pragma once

#include "bus/bus.h"
#include "cpu/cpu_types.h"
#include "disc/disc.h"

void init_cpu(CPU *self, Disc *disc, Bus *bus);
void run_background(CPU *self);

void set_cpu_helper(CPU *self);

void _helper_write_switch_from_exception(void);

void _helper_write_switch_to_exception(u32 cia);

void update_decrementor(CPU *cpu, u64 current, u64 next);
void handle_interrupt(CPU *self, enum CpuInternalInterruptType type);
