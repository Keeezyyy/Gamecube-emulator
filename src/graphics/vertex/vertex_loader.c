#include "vertex_loader.h"
#include "graphics/cp/cp.h"
#include <_abort.h>
#include <assert.h>
#include <stdbool.h>

static void _parse_pos(u32 VAT_A, u32 **stream, VertexPosition *p)
{
    p->has_position = true;
    p->position_elements = VAT_A & 1;
    p->position_data_type = (VAT_A >> 1) & 0x7;
    p->frac = (VAT_A >> 4) & 0x1F;

    assert(p->position_data_type < 5);

    u8 size = p->position_data_type == DATA_TYPE_F32 ? 4
              : p->position_data_type > DATA_TYPE_S8 ? 2
                                                     : 1; // size 1/2/4 bytes depentent on d_type

    u32 *xyz = &p->vec.X;
    for (int i = 0; i < (2 + (u8)p->position_elements); i++) {
        xyz[i] = read_stream(stream, size);
    }
}

static void _parse_norm(u32 VAT_A, u32 **stream, VertexNormal *n, bool is_direct)
{
    n->has_normal = true;

    n->normal_elements = (VAT_A >> 9) & 1;
    n->normal_data_type = (VAT_A >> 10) & 0x7;
    assert(n->normal_data_type == 1 || n->normal_data_type == 3 || n->normal_data_type == 4);

    u8 size = n->normal_data_type == DATA_TYPE_F32   ? 4
              : n->normal_data_type == DATA_TYPE_S16 ? 2
                                                     : 1; // size 1/2/4 bytes depentent on d_type

    Vec3 *vec = &n->N;
    if ((VAT_A >> 31) == 0 || is_direct) {
        for (u32 j = 0; j < ((n->normal_elements * 2) + 1); j++) {
            vec[j].X = read_stream(stream, size);
            vec[j].Y = read_stream(stream, size);
            vec[j].Z = read_stream(stream, size);
        }
    }
}

static void _parse_color(u32 VAT_A, u32 **stream, VertexColor *c, u8 color_idx)
{
    c->has_color = true;
    c->color_comp = (VAT_A >> (14 + color_idx * 4)) & 0x7;

    switch (c->color_comp) {
    case 0:
    case 3:
        // len = 2

        *((u16 *)c->color_buffer) = (u16)read_stream(stream, 2);
        break;
    case 1:
    case 4:
        // len = 3;
        ((u8 *)c->color_buffer)[2] = (u8)read_stream(stream, 1);
        ((u8 *)c->color_buffer)[1] = (u8)read_stream(stream, 1);
        ((u8 *)c->color_buffer)[0] = (u8)read_stream(stream, 1);
        break;
    case 2:
    case 5:
        // len = 4;
        *((u32 *)c->color_buffer) = (u32)read_stream(stream, 4);
        break;
    }
}

Vertex parse_vertex_from_stream(CPU *cpu, u32 VCD_HI, u32 VCD_LO, u32 VAT_A, u32 VAT_B, u32 VAT_C,
                                u32 CP_REGS[256], u32 **stream)
{
    Vertex out_v = {0};
    // Parse PosMat
    if (VCD_LO & 1) {
        out_v.pm.has_pos_mat_idx = true;
        out_v.pm.pos_mat_idx = (u8)read_stream(stream, 1);
    }
    // Parse TexMat
    for (int i = 0; i < 8; i++) {
        if ((VCD_LO >> (i + 1)) & 1) {
            out_v.tm[i].has_tex_mat_idx = true;
            out_v.tm[i].tex_mat_idx = (u8)read_stream(stream, 1);
        }
    }
    {
        // Parse Position
        const u8 pos_type = (VCD_LO >> 9) & 0b11;

        switch (pos_type) {
        case 0:
            out_v.pos.has_position = false;
            break;
        case 1:
            _parse_pos(VAT_A, stream, &out_v.pos);
            break;
        case 2:
        case 3:
            u16 idx = 0;
            idx = (u16)read_stream(stream, pos_type - 1);

            u32 *ram_stream =
                (u32 *)&cpu->bus->ram[(CP_REGS[0xA0] & 0x03FFFFFF) + idx * CP_REGS[0xB0] & 0xFF];
            _parse_pos(VAT_A, &ram_stream, &out_v.pos);
            break;
        }
    }

    // Normen Position
    {
        const u8 norm_type = (VCD_LO >> 11) & 0b11;

        switch (norm_type) {
        case 0:
            out_v.norm.has_normal = false;
            break;
        case 1:
            _parse_norm(VAT_A, stream, &out_v.norm, true);
            break;
        case 2:
        case 3:
            // NOTE: A 31 NormalIndex3 1 = bei NBT drei getrennte Indizes (NBT3)
            // can have 3 different indixes for vec3
            if (VAT_A >> 31) {
                _parse_norm(VAT_A, stream, &out_v.norm, false);

                u8 size = out_v.norm.normal_data_type == DATA_TYPE_F32   ? 4
                          : out_v.norm.normal_data_type == DATA_TYPE_S16 ? 2
                                                                         : 1;

                Vec3 *vec = &out_v.norm.N;
                for (u32 j = 0; j < ((out_v.norm.normal_elements * 2) + 1); j++) {
                    u16 idx = 0;
                    idx = (u16)read_stream(stream, norm_type - 1);

                    u32 *ram_stream = (u32 *)&cpu->bus->ram[CP_REGS[0xA1] + idx * CP_REGS[0xB1]];

                    vec[j].X = read_stream(&ram_stream, size);
                    vec[j].Y = read_stream(&ram_stream, size);
                    vec[j].Z = read_stream(&ram_stream, size);
                }
                break;

            } else {
                u16 idx = 0;
                idx = (u16)read_stream(stream, norm_type - 1);

                u32 *ram_stream =
                    (u32 *)&cpu->bus
                        ->ram[(CP_REGS[0xA1] & 0x03FFFFFF) + idx * CP_REGS[0xB1] & 0xFF];
                _parse_norm(VAT_A, &ram_stream, &out_v.norm, false);
                break;
            }
        }
    }

    //  Color 0/1

    for (u8 i = 0; i < 2; i++) {
        const u8 color_type = (VCD_LO >> (13 + i * 2)) & 0x3;

        switch (color_type) {
        case 0:
            out_v.color[i].has_color = false;
            break;
        case 1:
            _parse_color(VAT_A, stream, &out_v.color[i], i);
            break;
        case 2:
        case 3:
            u16 idx = 0;
            idx = (u16)read_stream(stream, color_type - 1);

            u32 *ram_stream =
                (u32 *)&cpu->bus
                    ->ram[(CP_REGS[0xA2 + i] & 0x03FFFFFF) + idx * CP_REGS[0xB2 + i] & 0xFF];

            _parse_color(VAT_A, &ram_stream, &out_v.color[i], i);
            break;
        }
    }
}
