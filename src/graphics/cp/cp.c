#include "cp.h"
#include "bus/bus.h"
#include "bus/interfaces/pi.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include "graphics/gpu/render/render.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static GXFifoRegs cp_regs;
static FifoControlRegisters fifo_ctr;

void cp_write(CPU *cpu, u32 adr, u32 val, u32 size)
{

    // pthread_mutex_lock(&cp_regs.gx_regs_mutex);

    printf("cp write : adr : 0x%08x, val : 0x%08x\n", adr, val);

    if (adr == 0xCC000004) {
        if (val & 1) {
            cp_regs.SR &= ~0x1;
        }
        if ((val >> 1) & 1) {
            cp_regs.SR &= ~0x2;
        }
        if ((val >> 2) & 1) {
            // assert(!"cp write clear metrics not implemented\n");
        }
    }

    assert(size == 2);

    volatile u8 *p = (volatile u8 *)&cp_regs;
    if (size == 2) {
        *(volatile u16 *)(p + (adr - 0xCC000000)) = (u16)val;
    } else {
        *(volatile u32 *)(p + (adr - 0xCC000000)) = (u32)val;
    }

    if (adr >= 0xCC000020 && adr <= 0xCC00003E) {
        atomic_store(&cp_regs.fifo_markers.FIFO_BASE, cp_regs._FIFO_BASE);
        atomic_store(&cp_regs.fifo_markers.FIFO_END, cp_regs._FIFO_END);
        atomic_store(&cp_regs.fifo_markers.READ_POINTER, cp_regs._READ_POINTER);
        atomic_store(&cp_regs.fifo_markers.WRITE_POINTER, cp_regs._WRITE_POINTER);
        atomic_store(&cp_regs.fifo_markers.RW_DISTANCE, cp_regs._RW_DISTANCE);
    }

    cp_check_state(cpu);

    // pthread_mutex_unlock(&cp_regs.gx_regs_mutex);
}
u64 cp_read(CPU *cpu, u32 adr, u32 size)
{

    // pthread_mutex_lock(&cp_regs.gx_regs_mutex);
    printf("read : 0x%08x\n", adr);
    volatile u8 *p = (volatile u8 *)&cp_regs;
    if (size == 2) {
        return *(volatile u16 *)(p + (adr - 0xCC000000));
    } else {

        return *(volatile u32 *)(p + (adr - 0xCC000000));
    }

    // pthread_mutex_unlock(&cp_regs.gx_regs_mutex);
}

void cp_check_state(CPU *cpu)
{
    u32 rw_distance = atomic_load(&cp_regs.fifo_markers.RW_DISTANCE);

    if (rw_distance > cp_regs.HI_WATERMARK) {
        cp_regs.SR |= 1;
    }
    if (rw_distance < cp_regs.LO_WATERMARK) {
        cp_regs.SR |= 2;
    }

    pi_update_interrupts(cpu);
}
u32 cp_get_cr_reg(void)
{
    // pthread_mutex_lock(&cp_regs.gx_regs_mutex);
    return cp_regs.CR;

    // pthread_mutex_unlock(&cp_regs.gx_regs_mutex);
}

u32 cp_get_sr_reg(void)
{

    // pthread_mutex_lock(&cp_regs.gx_regs_mutex);
    return cp_regs.SR;
    // pthread_mutex_unlock(&cp_regs.gx_regs_mutex);
}

void cp_recieve_gather_pipe(CPU *cpu)
{
    // read the data stream ...

    // pthread_mutex_lock(&cp_regs.gx_regs_mutex);

    atomic_fetch_add(&cp_regs.fifo_markers.WRITE_POINTER, 32);
    atomic_fetch_add(&cp_regs.fifo_markers.RW_DISTANCE, 32);
    // decode_data_stream(cpu, &cp_regs);

    u32 write_pointer = atomic_load(&cp_regs.fifo_markers.WRITE_POINTER);
    u32 fifo_end = atomic_load(&cp_regs.fifo_markers.FIFO_END);
    if (write_pointer > fifo_end) {
        u32 fifo_base = atomic_load(&cp_regs.fifo_markers.FIFO_BASE);
        atomic_store(&cp_regs.fifo_markers.WRITE_POINTER, fifo_base);
    }

    cp_check_state(cpu);

    pthread_mutex_lock(&fifo_ctr.fifo_mutex);
    pthread_cond_signal(&fifo_ctr.fifo_cond);
    pthread_mutex_unlock(&fifo_ctr.fifo_mutex);
}

GXFifoRegs *get_cp_regs(void)
{
    return &cp_regs;
}

void cp_init(CPU *cpu)
{
    pthread_cond_init(&fifo_ctr.fifo_cond, NULL);
    pthread_mutex_init(&fifo_ctr.fifo_mutex, NULL);
    init_vertex_loader(cpu, &cp_regs);
}

void cp_thread(CPU *cpu)
{

    init_renderer();
    // TODO:make fast thread save version
    // TODO:make fast thread save version
    // TODO:make fast thread save version
    u32 last_rw_distance = 0;
    while (true) {

        pthread_mutex_lock(&fifo_ctr.fifo_mutex);

        while (atomic_load(&cp_regs.fifo_markers.RW_DISTANCE) == last_rw_distance) {
            pthread_cond_wait(&fifo_ctr.fifo_cond, &fifo_ctr.fifo_mutex);
        }

    pthread_unlock_mutex:
        pthread_mutex_unlock(&fifo_ctr.fifo_mutex);

        decode_data_stream(cpu, &cp_regs);
        last_rw_distance = atomic_load(&cp_regs.fifo_markers.RW_DISTANCE);
    }
}
