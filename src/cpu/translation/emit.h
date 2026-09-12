#pragma once
#include "../cpu_types.h"
#include <stdbool.h>
u32 *emit_load_u64(u32 *out, u8 rd, u64 v);

u32 *emit_load_u32(u32 *out, u8 rd, u32 v);
