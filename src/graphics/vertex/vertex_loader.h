#pragma once

#include "bus/bus.h"
#include "core/config/config.h"
#include <assert.h>
#include <stdbool.h>

typedef struct {
    bool has_pos_mat_idx;
    u8 pos_mat_idx;
} PosMat;

typedef struct {
    bool has_tex_mat_idx;
    u8 tex_mat_idx;
} TexMat;

typedef enum {
    POS_ELEMNTS_XY = 0,
    POS_ELEMNTS_XYZ = 1,
} PositionFormatElements;

typedef enum {
    DATA_TYPE_U8 = 0,
    DATA_TYPE_S8 = 1,
    DATA_TYPE_U16 = 2,
    DATA_TYPE_S16 = 3,
    DATA_TYPE_F32 = 4,
} FormatDataType;

typedef u32 X32; // can hold types of PositionFormatDataType

typedef struct {
    X32 X;
    X32 Y;
    X32 Z;
} PACKED Vec3;

static_assert(sizeof(Vec3) == 3 * sizeof(u32));

typedef struct {
    bool has_position;
    PositionFormatElements position_elements;
    FormatDataType position_data_type;

    u8 frac; // Festkomma-Shift: Wert / 2^frac (nicht für f32

    Vec3 vec;
} PACKED VertexPosition;

typedef enum {
    NORMAL_ELEMENTS_N = 0,
    NORMAL_ELEMENTS_NBT = 1,
} NormalFormatElements;

typedef struct {
    bool has_normal;
    FormatDataType normal_data_type;
    NormalFormatElements normal_elements;

    Vec3 N;
    Vec3 B;
    Vec3 T;
} PACKED VertexNormal;

typedef struct {
    PosMat pm;
    TexMat tm[8];
    VertexPosition pos;
    VertexNormal norm;

} PACKED Vertex;

Vertex parse_vertex_from_stream(CPU *cpu, u32 VCD, u32 VAT_A, u32 VAT_B, u32 VAT_C,
                                u32 CP_REGS[256], u32 **stream);
