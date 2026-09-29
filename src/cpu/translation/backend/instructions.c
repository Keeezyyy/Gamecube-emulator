#include "instructions.h"
#include <oaknut_c.h>
#include "bus/bus.h"

u32 *translate_addi_func(TranslateFuncContext ctx)
{

    return ctx.code_buffer;
}
