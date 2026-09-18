#include "si.h"
#include "bus/interfaces/interface_utils.h"
#include "core/config/config.h"
#include <_abort.h>
#include <assert.h>

static u32 SIPOLL;
static u32 SICOMCSR;
static u32 SISR;

static u32 SI_BUF_CMD;

static u32 SIIOBUF_HI[4] = {0};
static u32 SIIOBUF_LO[4] = {0};

static void si_start_transfer(u32 val)
{
    SICOMCSR = val;
    switch (SI_BUF_CMD) {
    case SI_CMD_STATUS_REQ: {
    }
        SIIOBUF_HI[0] = SI_TYPE_GC_CONTROLLER;
        SIIOBUF_HI[1] = SI_TYPE_NOT_CONNECTED;
        SIIOBUF_HI[2] = SI_TYPE_NOT_CONNECTED;
    }
    SICOMCSR &= ~((1 << 0) | (1 << 0));
    SICOMCSR |= ((1 << 31));
}

void si_write(CPU *cpu, u32 adr, u64 val, u32 size)
{
    if (adr == 0xCC006400 + 0x30) {
        SIPOLL = val & U32_MAX;
        return;
    }
    if (adr == 0xCC006400 + 0x34) {
        if (val & 1) {
            // start transfer
            si_start_transfer(val);
        } else {
            SICOMCSR &= W1C(val, 31);
        }

        return;
    }
    if (adr == 0xCC006400 + 0x38) {
        for (int channel = 0; channel < 4; channel++) {
            for (int bit = 0; bit < 4; bit++) {
                SISR &= W1C(val, channel * 8 + bit);
            }
        }
        if ((val >> 31) & 1) {
            assert(!"buffer übernehmen\n");
        }

        return;
    }
    if (adr == 0xCC006400 + 0x80) {
        SI_BUF_CMD = val;

        return;
    }
    assert(!"si_write: unhandled SI register address");
}

u64 si_read(CPU *cpu, u32 adr, u64 val)
{
    if (adr == 0xCC006400 + 0x34) {
        return SICOMCSR;
    } else if (adr == 0xCC006400 + 0x38) {
        return SISR;
    }
    assert(!"si_read: unhandled SI register address");
}
