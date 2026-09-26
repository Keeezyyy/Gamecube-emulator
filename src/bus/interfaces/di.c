#include "di.h"
#include "bus/bus.h"
#include "bus/interfaces/interface_utils.h"
#include "bus/interfaces/pi.h"
#include "core/config/config.h"
#include <arm/types.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static u32 disc_register;
static u32 disc_cover_register;

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
        printf("[DI-READ] :  adr : 0x%08x, size : %d\n", adr, size);

    switch (adr) {
    case 0xCC006000:
        return disc_register;
    case 0xCC006004:
        // return disc_cover_register | 1;
        return disc_cover_register | 1;
    case 0xCC006018:
        return di_dma_regs.DILENGTH;
    case 0xCC006024:
        return 1;
    }
    assert(!"dvd");
}

void di_write(CPU *cpu, u32 adr, u32 val, u32 size)
{
    printf("[DI-WRITE] :  adr : 0x%08x, val : 0x%08x,size : %d\n", adr, val, size);
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
typedef struct {
    char gamename[4];      // 0x00: "GALE"
    char company[2];       // 0x04: "01"
    uint8_t disknum;       // 0x06
    uint8_t gamever;       // 0x07
    uint8_t streaming;     // 0x08: 0 / 1
    uint8_t streambufsize; // 0x09
    uint8_t pad[22];       // 0x0A - 0x1F
    uint32_t magic;        // 0x20: 0xC2339F3D
} dvd_disk_id_t;

static const dvd_disk_id_t dvddiskid = {
    .gamename = {'G', 'A', 'L', 'E'},
    .company = {'0', '1'},
    .disknum = 0,
    .gamever = 0,
    .streaming = 0,
    .streambufsize = 0,
    .pad = {0},
    .magic = 0xC2339F3D,
};

void di_start_dma(CPU *cpu)
{
    printf("DVD DMA : 0x%08x\n", di_dma_regs.DICMDBUF0);
    u8 op = (di_dma_regs.DICMDBUF0 >> 24) & 0xFF;
    switch (op) {
    case 0xA8: {
        if ((op & 0xFF) == 0x00) {
            // DVD_READSECTOR
            assert(!"dvd read sector\n");
        } else {
            // DVD_READDISKID

            memcpy(((u8 *)cpu->bus->ram) + (di_dma_regs.DIMAR - 0x80000000), &dvddiskid,
                   sizeof(dvddiskid));

            disc_register |= BIT(4);
            pi_update_interrupts(cpu);
            printf("DVD DMA read disk id\n");
        }
        break;
    }
    case 0xE3: {

        printf("DVD STOP MOTOR\n");
        disc_register |= BIT(4);
        pi_update_interrupts(cpu);
        break;
    }
    }
    di_dma_regs.DILENGTH = 0;
}
