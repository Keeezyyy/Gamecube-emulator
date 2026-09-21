#include "gx_fifo.h"
#include "core/config/config.h"
#include "cp/cp.h"
#include "bus/interfaces/pi.h"
#include "cpu/cpu_types.h"
#include <stdio.h>
#include <string.h>

union FifoBuffer {
    u32 buffer_32[16];
    u16 buffer_16[32];
    u8 buffer_8[64];
}; // NORMALLY 32 bytes of buffer storage (for security 2x)
static union FifoBuffer fifo_buffer;

u32 buffer_ptr;

static void copy_fifo_buffer_to_ram(CPU *cpu) {};

void gx_write_to_fifo(CPU *cpu, u64 val, u32 size)
{
    for (u32 i = 0; i < size; i++)
        fifo_buffer.buffer_8[buffer_ptr++] = (u8)(val >> ((size - 1 - i) * 8));

    if (buffer_ptr >= 32) {
        // FIFO BUFFER IS FULL

        // send_to_gpu()

        // send to cp
        pi_recieve_gx_gather_piper(cpu, fifo_buffer.buffer_32);

        buffer_ptr -= 32;
        memcpy(fifo_buffer.buffer_8, fifo_buffer.buffer_8 + 32, buffer_ptr);
        GPU_PRINT("[GX] FIFO BUFFER FULL\n");
    }
}
