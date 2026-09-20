#pragma once

#include "core/config/config.h"
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
    POS_DATA_TYPE_U8 = 0,
    POS_DATA_TYPE_S8 = 1,
    POS_DATA_TYPE_U16 = 2,
    POS_DATA_TYPE_S16 = 3,
    POS_DATA_TYPE_F32 = 4,
} PositionFormatDataType;

typedef u32 X32; // can hold types of PositionFormatDataType

typedef struct {
    bool has_position;
    PositionFormatElements position_elements;
    PositionFormatDataType position_data_type;
    u8 frac; // Festkomma-Shift: Wert / 2^frac (nicht für f32

    X32 X;
    X32 Y;
    X32 Z;
} PACKED VertexPosition;

typedef struct {
    PosMat pm;
    TexMat tm[8];
    VertexPosition pos;

} PACKED Vertex;

Vertex parse_vertex_from_stream(u32 VCD, u32 VAT_A, u32 VAT_B, u32 VAT_C, u32 **stream);
