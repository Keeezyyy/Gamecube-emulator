#include "gx_fifo.h"
#include "bus/hardware_registers.h"
#include "bus/interfaces/pi.h"
#include "cpu/cpu_types.h"
#include <stdio.h>

union FifoBuffer {
    u32 buffer_32[16];
    u16 buffer_16[32];
    u8 buffer_8[64];
}; // NORMALLY 32 bytes of buffer storage (for security 2x)
static union FifoBuffer fifo_buffer;

u32 buffer_ptr;

static void copy_fifo_buffer_to_ram(CPU *cpu) {};

void gx_write_to_fifo(CPU *cpu, u32 val, u32 size)
{
    switch (size) {
    case 1:
        ((u8 *)&fifo_buffer)[buffer_ptr++] = (u8)val;
        break;
    case 2:
        ((u16 *)&fifo_buffer)[buffer_ptr] = (u16)val;
        buffer_ptr += 2;
        break;
    case 3:
        ((u32 *)&fifo_buffer)[buffer_ptr] = (u32)val;
        buffer_ptr += 4;
        break;
    }

    if (buffer_ptr >= 32) {
        // FIFO BUFFER IS FULL

        // send_to_gpu()

        // send to cp
        pi_recieve_gx_gather_piper(cpu, &fifo_buffer);

        buffer_ptr = 0;
        printf("[GX] FIFO BUFFER FULL\n");
    }
}
