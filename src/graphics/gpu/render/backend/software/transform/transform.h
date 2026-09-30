#pragma once

#include "core/config/config.h"
#include "graphics/gpu/render/xf_types.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <assert.h>

typedef struct {
    Float4 pos;
    Float3 eye;
    Float3 NBT[3];
    Float3 tex[8];

    RGBA colors[2];

} XFOutput;

void software_backend_write_to_xf_reg(const u32 reg_num, const u32 val);
XFOutput transform_vertex(CPU *cpu, const Vertex *v);

const XF_Registers *get_xf_regs(void);
const u32 *software_backend_get_xf_buffer(void);
