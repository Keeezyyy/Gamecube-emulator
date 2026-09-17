#include "core/config/config.h"
#include "cpu.h"
#include "cpu/cpu_types.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/_pthread/_pthread_mutex_t.h>

#define GEKKO_TB_HZ 40500000ULL
static uint64_t arm_freq;
static uint64_t arm_base;
static uint64_t gekko_base;

static uint64_t mult;

static inline uint64_t read_cntvct(void)
{
    uint64_t v;
    __asm__ volatile("mrs x0, cntvct_el0\nstr x0, [%0]" ::"r"(&v) : "x0", "memory");
    return v;
}
static inline uint64_t read_cntfrq(void)
{
    uint64_t v;
    __asm__ volatile("mrs x0, cntfrq_el0\nstr x0, [%0]" ::"r"(&v) : "x0", "memory");
    return v;
}
pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;

void run_background(CPU *self)
{
    arm_freq = read_cntfrq();
    mult = (uint64_t)(((unsigned __int128)GEKKO_TB_HZ << 32) / arm_freq);
    while (true) {
        uint64_t delta = read_cntvct() - arm_base;
        gekko_base = (uint64_t)(((unsigned __int128)delta * mult) >> 32);

        pthread_mutex_lock(&my_mutex);
        self->special_purpose_registers.buf[268] = gekko_base & U32_MAX;
        self->special_purpose_registers.buf[269] = gekko_base >> 32;

        // debug checks for not implemented machine states

        assert(((self->state.msr >> 17) & 1) == 0);
        assert(((self->state.msr >> 26) & 1) == 0);
        assert(((self->state.msr >> 27) & 1) == 0);

        pthread_mutex_unlock(&my_mutex);
    }
}
