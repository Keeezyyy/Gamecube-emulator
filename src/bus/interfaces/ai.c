#include "ai.h"
#include <assert.h>

static u64 dsp_csr = 0;
u64 ai_read(CPU *cpu, u32 adr, u32 size)
{
    if (adr == 0xCC00500A) {
        return dsp_csr;
    }

    assert(!"sound not implemented");
    return 0;
}

void ai_write(CPU *cpu, u32 adr, u64 val, u32 size)
{
    u32 value = (u32)val;
    dsp_csr &= ~(val & ((1 << 7) | (1 << 5) | (1 << 3)));
    dsp_csr = (dsp_csr & ~((1 << 8) | (1 << 6) | (1 << 4) | (1 << 2))) |
              (value & ((1 << 8) | (1 << 6) | (1 << 4) | (1 << 2)));
    if (value & 1) {

        // dsp_reset();
    }
    if (value & (1 << 1)) {
        // dsp_raise(DSPINT);
    }
    // dsp_update_interrupt();
}

static u64 audio_interface_control_register = 0;
static u64 audio_interface_sample_counter = 0;
void ai_write_to_streaming_interface(CPU *cpu, u32 adr, u64 val, u32 size)
{

    u32 value = (u32)val;

    audio_interface_control_register &= ~(val & ((1 << 3)));
    if (val & (1 << 5)) {
        // 5 SCRESET 1 schreiben = AISCNT auf 0 setzen
        audio_interface_sample_counter = 0;
    }
}

u64 ai_read_from_streaming_interface(CPU *cpu, u32 adr, u32 size)
{
    return audio_interface_control_register;
}
