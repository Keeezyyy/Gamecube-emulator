
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/_pthread/_pthread_t.h>

#include "./disc/disc.h"
#include "bus/bus.h"
#include "cpu/cpu.h"
#include "cpu/cpu_types.h"
#include "graphics/vi.h"

static Bus b;
static Disc disc;
static CPU cpu;

#ifdef CLOCK_STATS
#include "scheduler/scheduler.h"
#include <signal.h>
#include <time.h>
#include <unistd.h>

static struct timespec t0;

static void print_clock(int sig)
{
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double s = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    double hz = (double)global_cycle_counter / s;
    printf("\n[CLOCK] %.2f MHz avg ueber %.2f s (%.2f%% von %u MHz)\n", hz / 1e6, s,
           hz / CPU_CLOCK_SPEED * 100.0, CPU_CLOCK_SPEED / 1000000u);
    fflush(stdout);
    _exit(128 + sig);
}
#endif

static int _init(void)
{

    init_bus(&b);

    b.set_cpu_ptr(&b, &cpu);
    init_disc(&disc);
    if (disc.load_rom(&disc, "./roms/example.bin") != 0) {
        return 1;
    }
    // load the decrypted rom
    // NOTE: might implement encryption
    if (b.load_ipl(&b, "./roms/ngc_pal_ipl.dol") != 0) {
        return 1;
    }
    if (b.load_ipl_scrambled(&b, "./roms/ipl.bin") != 0) {
        return 1;
    }

    disc.print_header(&disc);

    init_cpu(&cpu, &disc, &b);

    return 0;
}

static void _free(void)
{
    b.free(&b);
    disc.free(&disc);
    cpu.free(&cpu);
}

int main(int argc, char **argv)
{

    if (_init() != 0) {
        _free();
        return 1;
    }

    cpu.boot(&cpu);

#ifdef CLOCK_STATS
    clock_gettime(CLOCK_MONOTONIC, &t0);
    signal(SIGINT, print_clock);
    signal(SIGABRT, print_clock);
#endif

    cpu.main(&cpu);

    while (true) {
    }
}
