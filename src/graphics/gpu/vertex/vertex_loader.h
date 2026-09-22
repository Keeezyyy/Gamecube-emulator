#pragma once

#include "bus/bus.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
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

typedef struct {
    X32 S;
    X32 T;
} PACKED TexVec2;

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

typedef enum {
    RGB565 = 0,
    RGB888 = 1,
    RGB888x = 2,
    RGBA4444 = 3,
    RGBA6666 = 4,
    RGBA8888 = 5
} VertexColorComp;

typedef struct {
    bool has_color;
    VertexColorComp color_comp;
    u8 rgba[4];

} PACKED VertexColor;

typedef enum {
    TEX_ELEMNTS_S = 0,
    TEX_ELEMNTS_ST = 1,
} TextureFormatElements;

typedef struct {
    bool has_texture;
    TextureFormatElements texture_elements;
    FormatDataType texture_data_type;

    u8 frac; // Festkomma-Shift: Wert / 2^frac (nicht für f32
    TexVec2 elemets;

} PACKED VertexTexture;

typedef enum {
    GX_QUADS = 0,
    GX_QUADS_2 = 1,
    GX_TRIANGLES = 2,
    GX_TRIANGLESTRIP = 3,
    GX_TRIANGLEFAN = 4,
    GX_LINES = 5,
    GX_LINESTRIP = 6,
    GX_POINTS = 7
} PrimitiveType;

typedef struct {
    PosMat pm;
    TexMat tm[8];
    VertexPosition pos;
    VertexNormal norm;
    VertexColor color[2];
    VertexTexture texture[8];

    PrimitiveType type;

} PACKED Vertex;

Vertex parse_vertex_from_stream(CPU *cpu, u32 VCD_HI, u32 VCD_LO, u32 VAT_A, u32 VAT_B, u32 VAT_C,
                                u32 CP_REGS[256], u8 **stream, PrimitiveType type);

void init_vertex_loader(CPU *cpu, GXFifoRegs *command_processor_registers);
