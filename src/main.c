
#include <stdio.h>
#include <string.h>

#include "./disc/disc.h"
#include "bus/bus.h"
#include "cpu/cpu.h"
#include "cpu/cpu_types.h"

static Bus b;
static Disc disc;
static CPU cpu;

static int _init(void)
{
    init_bus(&b);
    init_disc(&disc);
    if (disc.load_rom(&disc, "./roms/example.bin") != 0) {
        return 1;
    }
    // load the decrypted rom
    // NOTE: might implement encryption
    if (b.load_ipl(&b, "./roms/ngc_pal_ipl.dol") != 0) {
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

    cpu.main(&cpu);

    return 0;
}
