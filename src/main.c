
#include <stdio.h>
#include <string.h>

#include "./disc/disc.h"
#include "cpu/cpu.h"
#include "cpu/cpu_types.h"

int main(int argc, char **argv)
{
    load_rom("./roms/example.bin");

    CPU cpu;
    init_cpu(&cpu);

    cpu.free(&cpu);
}
