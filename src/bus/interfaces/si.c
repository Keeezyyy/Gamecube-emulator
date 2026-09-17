#include "si.h"
#include "bus/interfaces/interface_utils.h"
#include "core/config/config.h"
#include <_abort.h>
#include <assert.h>

static u32 SIPOLL;
static u32 SICOMCSR;
void si_write(CPU *cpu, u32 adr, u64 val, u32 size)
{
    if (adr == 0xCC006400 + 0x30) {
        SIPOLL = val & U32_MAX;
        return;
    }
    if (adr == 0xCC006400 + 0x34) {
        SICOMCSR &= W1C(val, 31);
        return;
    }
    assert(!"si_write: unhandled SI register address");
}

u64 si_read(CPU *cpu, u32 adr, u64 val)
{
    if (adr == 0xCC006400 + 0x34) {
        return SICOMCSR;
    }
    assert(!"si_read: unhandled SI register address");
}
