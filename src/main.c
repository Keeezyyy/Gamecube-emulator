
#include <stdio.h>
#include <string.h>

#include "./disc/disc.h"
#include "bus/bus.h"
#include "cpu/cpu.h"
#include "cpu/cpu_types.h"

int main(int argc, char **argv)
{

    Bus b;
    Disc disc;
    CPU cpu;

    init_bus(&b);
    init_disc(&disc);
    if (disc.load_rom(&disc, "./roms/example.bin") != 0) {
        return 1;
    }
    // load the encrypted rom
    // NOTE: might implement encryption
    if (b.load_bios(&b, "./roms/ipl.bin") != 0) {
        return 1;
    }

    disc.print_header(&disc);

    init_cpu(&cpu, &disc);

    {
        b.free(&b);
        disc.free(&disc);
        cpu.free(&cpu);
    }
    return 0;
}
