#include "di.h"
#include "bus/bus.h"
#include "bus/interfaces/interface_utils.h"
#include "bus/interfaces/pi.h"
#include "core/config/config.h"
#include "cpu/translation/translation.h"
#include "disc/disc.h"
#include "scheduler/scheduler.h"
#include <arm/types.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static Disc *disc_manager_ptr;

static u32 disc_register;
static u32 disc_cover_register;
static bool audio_streaming_active = false;

static SchedulerOneTimeEvent e;

typedef struct {
    volatile uint32_t DICMDBUF0; // 0x00: Befehl (Byte 3 = Opcode, Byte 2 = Subkommando)
    volatile uint32_t DICMDBUF1; // 0x04: Parameter 1 (meist Offset / 4)
    volatile uint32_t DICMDBUF2; // 0x08: Parameter 2 (meist Länge)
    volatile uint32_t DIMAR;     // 0x0C: DMA-Zieladresse im RAM
    volatile uint32_t DILENGTH;  // 0x10: DMA-Länge (zählt herunter)
    volatile uint32_t DICR;
} DI_Regs;

static DI_Regs di_dma_regs;

u64 di_read(CPU *cpu, u32 adr, u32 size)
{

    if (adr != 0xcc006004)
        DI_PRINT("[DI-READ] :  adr : 0x%08x, size : %d\n", adr, size);

    switch (adr) {
    case 0xCC006000:
        return disc_register;
    case 0xCC006004:
        return disc_cover_register;
        return disc_cover_register;
    case 0xCC006018:
        return di_dma_regs.DILENGTH;
    case 0xCC006024:
        return 1;
    }
    assert(!"dvd");
}

void di_write(CPU *cpu, u32 adr, u32 val, u32 size)
{
    DI_PRINT("[DI-WRITE] :  adr : 0x%08x, val : 0x%08x,size : %d\n", adr, val, size);
    assert(size == 4);
    switch (adr) {
    case 0xCC006000: {
        int w1cs[] = {2, 4, 6};
        set_register(&disc_register, val, w1cs, ARRAY_SIZE(w1cs));
        pi_update_interrupts(cpu);
        return;
    }
    case 0xCC006004: {
        // DISC COVER REGISTER
        int w1cs[] = {2};
        set_register_read_only(&disc_cover_register, val, w1cs, ARRAY_SIZE(w1cs), BIT(0));
        pi_update_interrupts(cpu);
        return;
    }
    case 0xCC006008: {
        di_dma_regs.DICMDBUF0 = val;
        return;
    }
    case 0xCC00600C: {
        di_dma_regs.DICMDBUF1 = val;
        return;
    case 0xCC006010: {
        di_dma_regs.DICMDBUF2 = val;
        return;
    }
    case 0xCC006014: {
        di_dma_regs.DIMAR = val;
        return;
    }
    case 0xCC006018: {
        di_dma_regs.DILENGTH = val;
        return;
    }
    case 0xCC00601C: {

        di_dma_regs.DICR = val & ~0x1;
        if (val & 1) {
            // DMA START
            di_start_dma(cpu);
        }

        return;
    }
    }
    }

    assert(!"dvd not implemented");
}
u32 di_get_disr(void)
{
    return disc_register;
}

u32 di_get_dicvr(void)
{
    return disc_cover_register;
}

static const dvd_disk_id_t dvddiskid = {
    .gamename = {'G', 'A', 'L', 'E'},
    .company = {'0', '1'},
    .disknum = 0,
    .gamever = 0,
    .streaming = 0,
    .streambufsize = 0,
    .pad = {0},
    .magic = __builtin_bswap32(0xC2339F3D),
};

static_assert(sizeof(dvd_disk_id_t) == 0x20);

static void _read_dvd(CPU *cpu, u32 disk_offset, u32 len, u32 adr_virtual)
{
    disc_manager_ptr->read(disc_manager_ptr, &cpu->bus->ram[adr_virtual - 0x80000000],
                           disk_offset << 2, len);

    invalidate_tb(adr_virtual, adr_virtual + len);
}

void di_start_dma(CPU *cpu)
{
    DI_PRINT("[DI] DVD DMA : 0x%08x\n", di_dma_regs.DICMDBUF0);
    u32 cmd = (di_dma_regs.DICMDBUF0);
    u8 op = (cmd >> 24) & 0xFF;
    printf("cmd : 0x%08x, op : 0x%02x\n", cmd, op);
    switch (op) {
    case 0xA8: {
        if ((cmd & 0xFF) == 0x0) {
            // DVD_READSECTOR
            const u32 disk_offset = di_dma_regs.DICMDBUF1;
            const u32 len = di_dma_regs.DICMDBUF2;
            const u32 adr_virtual = di_dma_regs.DIMAR;

            printf("[DVD_READ] : disk adr : 0x%08x, len : 0x%08x, dest_adr_virtual : 0x%08x\n",
                   disk_offset, len, adr_virtual);

            _read_dvd(cpu, disk_offset, len, adr_virtual);
            di_dma_regs.DICR |= BIT(0);

            // TODO: calc the actual timing fo read amount , ...
            scheduler_activate_one_time_event(SCHEDULER_ONE_TIME_EVENT_DVD_READ, 1);

        } else {
            // DVD_READDISKID

            // memcpy(((u8 *)cpu->bus->ram) + (di_dma_regs.DIMAR - 0x80000000), &dvddiskid,
            // sizeof(dvddiskid));

            _read_dvd(cpu, 0, 0x20, di_dma_regs.DIMAR);
            disc_register |= BIT(4);
            pi_update_interrupts(cpu);
            DI_PRINT("[DI] DVD DMA read disk id\n");
        }
        break;
    }
    case DI_CMD_DVD_AUDIOCONFIG: {
        if (cmd == 0xE4000000)
            audio_streaming_active = false;
        else
            audio_streaming_active = true;
        disc_register |= BIT(4);
        pi_update_interrupts(cpu);
        DI_PRINT("[DI] DVD_LowAudioBufferConfig\n");
        break;
    }
    case 0xE3: {

        DI_PRINT("[DI] DVD STOP MOTOR\n");
        disc_register |= BIT(4);
        pi_update_interrupts(cpu);
        break;
    }
    case 0x12: {
        DI_PRINT("[DI] DVD INQUIRY\n");

        u8 buffer[32] = {0};
        ((u16 *)buffer)[0] = __builtin_bswap16(0x0002);
        ((u16 *)buffer)[1] = __builtin_bswap16(0x0002);
        ((u32 *)buffer)[1] = __builtin_bswap32(0x20020823);

        memcpy(((u8 *)cpu->bus->ram) + (di_dma_regs.DIMAR - 0x80000000), buffer, 32);

        disc_register |= BIT(4);
        pi_update_interrupts(cpu);
        break;
    }
    default:
        assert(!"di not implemented");
    }
    di_dma_regs.DILENGTH = 0;
}
static void _di_scheduler_event_callback_di_read(CPU *cpu)
{
    di_dma_regs.DICR &= ~BIT(0);
    disc_register |= BIT(4);
    pi_update_interrupts(cpu);
}

void di_init(Disc *d)
{
    disc_manager_ptr = d;
    e.active = false;
    e.activate_on_cycle = 0;
    e.callback = &_di_scheduler_event_callback_di_read;

    scheduler_add_one_time_event(SCHEDULER_ONE_TIME_EVENT_DVD_READ, e);
}
