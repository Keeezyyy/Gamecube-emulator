#pragma once
#include "graphics/gpu/render/backend/software/rasterize/rasterize.h"
typedef enum { Color0 = 0, Color1 = 1, AlphaBump = 5, AlphaBumpN = 6, Zero = 7 } ColorType;

typedef struct {
    s16 r;
    s16 g;
    s16 b;
    s16 a;
} RGBAS16;
enum CombineBias {
    BIAS_ZERO = 0,
    BIAS_PLUS = 1,
    BIAS_MINUS = 2,
    BIAS_COMPARE = 3,
};

enum CombineOperation {
    OP_ADD = 0,
    OP_SUB = 1,
};

enum CombineClamp {
    CLAMP = 0,
    NO_CLAMP = 1,
};

enum CombineScale {
    SCALE_X1 = 0,
    SCALE_X2 = 1,
    SCALE_X4 = 2,
    SCALE_DIV2 = 3,
};

enum CombineDest {
    DEST_PREV = 0,
    DEST_REG0 = 1,
    DEST_REG1 = 2,
    DEST_REG2 = 3,
};

typedef struct {
    enum CombineBias bias;
    enum CombineOperation operation;
    enum CombineClamp clamp;
    enum CombineScale scale;
    enum CombineDest dest;
} CombineConfig;

void tev_write_color_reg(u8 reg, u32 value);

void draw_pixel(CPU *cpu, const PixelAttributes *p, u32 x, u32 y, u32 ox, u32 oy);
