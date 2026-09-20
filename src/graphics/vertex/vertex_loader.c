#include "vertex_loader.h"
#include "graphics/cp/cp.h"
#include <_abort.h>
#include <assert.h>

static void _parse_pos(u32 VAT_A, u32 **stream, VertexPosition *p)
{
    p->has_position = true;
    p->position_elements = VAT_A & 1;
    p->position_data_type = (VAT_A >> 1) & 0x7;
    p->frac = (VAT_A >> 4) & 0xF;

    assert(p->position_data_type < 5);

    u8 size = p->position_data_type == DATA_TYPE_F32 ? 4
              : p->position_data_type > DATA_TYPE_S8 ? 2
                                                     : 1; // size 1/2/4 bytes depentent on d_type

    u32 *xyz = &p->vec.X;
    for (int i = 0; i < (2 + (u8)p->position_elements); i++) {
        xyz[i] = read_stream(stream, size);
    }
}

static void _parse_norm(u32 VAT_A, u32 **stream, VertexNormal *n)
{
    n->has_normal = true;

    n->normal_elements = (VAT_A >> 9) & 1;
    n->normal_data_type = (VAT_A >> 10) & 0x3;
    assert(n->normal_data_type == 1 || n->normal_data_type == 3 || n->normal_data_type == 4);

    u8 size = n->normal_data_type == DATA_TYPE_F32   ? 4
              : n->normal_data_type == DATA_TYPE_S16 ? 2
                                                     : 1; // size 1/2/4 bytes depentent on d_type

    Vec3 *vec = &n->N;
    if ((VAT_A >> 31) == 0) {
        for (u32 j = 0; j < ((n->normal_elements * 2) + 1); j++) {
            vec[j].X = read_stream(stream, size);
            vec[j].Y = read_stream(stream, size);
            vec[j].Z = read_stream(stream, size);
        }
    }
}

Vertex parse_vertex_from_stream(CPU *cpu, u32 VCD, u32 VAT_A, u32 VAT_B, u32 VAT_C,
                                u32 CP_REGS[256], u32 **stream)
{
    Vertex out_v = {0};
    // Parse PosMat
    if (VCD & 1) {
        out_v.pm.has_pos_mat_idx = true;
        out_v.pm.pos_mat_idx = (u8)read_stream(stream, 1);
    }
    // Parse TexMat
    for (int i = 0; i < 8; i++) {
        if ((VCD >> (i + 1)) & 1) {
            out_v.tm[i].has_tex_mat_idx = true;
            out_v.tm[i].tex_mat_idx = out_v.pm.pos_mat_idx = (u8)read_stream(stream, 1);
        }
    }
    {
        // Parse Position
        const u8 pos_type = (VCD >> 9) & 0b11;

        switch (pos_type) {
        case 0:
            out_v.pos.has_position = false;
            break;
        case 1:
            _parse_pos(VAT_A, stream, &out_v.pos);
            break;
        case 2:
            u16 idx = 0;
            idx = (u16)read_stream(stream, pos_type - 1);

            u32 *ram_stream = (u32 *)&cpu->bus->ram[CP_REGS[0xA0] + idx * CP_REGS[0xB0]];
            _parse_pos(VAT_A, &ram_stream, &out_v.pos);
            break;
        }
    }

    // Normen Position
    {
        const u8 norm_type = (VCD >> 11) & 0b11;

        switch (norm_type) {
        case 0:
            out_v.norm.has_normal = false;
            break;
        case 1:
            _parse_norm(VAT_A, stream, &out_v.norm);
            break;
        case 2:
            // NOTE: A 31 NormalIndex3 1 = bei NBT drei getrennte Indizes (NBT3)
            // can have 3 different indixes for vec3
            if (VAT_A >> 31) {
                _parse_norm(VAT_A, stream, &out_v.norm);

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

                u32 *ram_stream = (u32 *)&cpu->bus->ram[CP_REGS[0xA1] + idx * CP_REGS[0xB1]];
                _parse_norm(VAT_A, &ram_stream, &out_v.norm);
                break;
            }
        }
    }
}
