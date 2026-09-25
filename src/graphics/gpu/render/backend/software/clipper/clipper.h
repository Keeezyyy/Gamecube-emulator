#pragma once

#include "core/config/config.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"

#define MAX_CLIP_VERTS 16

typedef struct {
    Float3 v[3];
} Triangle;

bool clipping(XFOutput *clipping_in1, XFOutput *clipping_in2, XFOutput *clipping_in3,
              const XF_Registers *xf_regs, XFOutput *out, u16 *polygon_count);

bool clip_line(XFOutput *clipping_in1, XFOutput *clipping_in2, const XF_Registers *xf_regs,
               XFOutput *out_1, XFOutput *out_2);
bool clip_dot(XFOutput *clipping_in1, const XF_Registers *xf_regs);
