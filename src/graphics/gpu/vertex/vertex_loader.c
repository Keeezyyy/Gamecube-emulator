#include "vertex_loader.h"
#include "graphics/cp/cp.h"
#include <_abort.h>
#include <assert.h>
#include <stdbool.h>

static CPU *static_cpu_ptr;
static GXFifoRegs *gx_fifo_regs;

void init_vertex_loader(CPU *cpu, GXFifoRegs *command_processor_registers)
{
    static_cpu_ptr = cpu;
    gx_fifo_regs = command_processor_registers;
}

static void _parse_pos(u32 VAT_A, u8 **stream, VertexPosition *p)
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
        xyz[i] = read_stream(static_cpu_ptr, gx_fifo_regs, stream, size);
    }
}

static void _parse_norm(u32 VAT_A, u8 **stream, VertexNormal *n, bool is_direct)
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
            vec[j].X = read_stream(static_cpu_ptr, gx_fifo_regs, stream, size);
            vec[j].Y = read_stream(static_cpu_ptr, gx_fifo_regs, stream, size);
            vec[j].Z = read_stream(static_cpu_ptr, gx_fifo_regs, stream, size);
        }
    }
}

static void _parse_color(u32 VAT_A, u8 **stream, VertexColor *c, u8 color_idx)
{
    c->has_color = true;
    c->color_comp = (VAT_A >> (14 + color_idx * 4)) & 0x7;

    u8 *p = c->rgba;

    switch (c->color_comp) {
    case RGB565: {
        u16 v = (u16)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 2);
        u8 r = (u8)((v >> 11) & 0x1F), g = (u8)((v >> 5) & 0x3F), b = (u8)(v & 0x1F);
        p[0] = (u8)(r << 3 | r >> 2);
        p[1] = (u8)(g << 2 | g >> 4);
        p[2] = (u8)(b << 3 | b >> 2);
        p[3] = 0xFF;
        break;
    }
    case RGB888x:
    case RGB888:
        p[0] = (u8)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 1);
        p[1] = (u8)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 1);
        p[2] = (u8)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 1);
        p[3] = 0xFF;
        if (c->color_comp == RGB888x)
            read_stream(static_cpu_ptr, gx_fifo_regs, stream, 1);
        break;
    case RGBA4444: {
        u16 v = (u16)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 2);
        for (u8 i = 0; i < 4; i++)
            p[i] = (u8)(((v >> (12 - i * 4)) & 0xF) * 0x11);
        break;
    }
    case RGBA6666: {
        u32 v = (u32)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 1) << 16;
        v |= (u32)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 1) << 8;
        v |= (u32)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 1);
        for (u8 i = 0; i < 4; i++) {
            u8 x = (u8)((v >> (18 - i * 6)) & 0x3F);
            p[i] = (u8)(x << 2 | x >> 4);
        }
        break;
    }
    case RGBA8888:
        for (u8 i = 0; i < 4; i++)
            p[i] = (u8)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 1);
        break;
    default:
        assert(!"wrong color comp\n");
    }
}

static u32 _vat_bits(const u32 *VAT, u8 bit, u8 n)
{
    return (VAT[bit >> 5] >> (bit & 31)) & ((1u << n) - 1);
}

static void _parse_tex(const u32 *VAT, u8 **stream, VertexTexture *t, u8 t_idx)
{
    static const u8 elem_bit[8] = {21, 32, 41, 50, 59, 69, 78, 87};
    static const u8 frac_bit[8] = {25, 36, 45, 54, 64, 73, 82, 91};

    t->has_texture = true;
    t->texture_elements = _vat_bits(VAT, elem_bit[t_idx], 1);
    t->texture_data_type = _vat_bits(VAT, (u8)(elem_bit[t_idx] + 1), 3);
    t->frac = (u8)_vat_bits(VAT, frac_bit[t_idx], 5);

    assert(t->texture_data_type < 5);

    u8 size = t->texture_data_type == DATA_TYPE_F32 ? 4
              : t->texture_data_type > DATA_TYPE_S8 ? 2
                                                    : 1;

    t->elemets.S = (X32)read_stream(static_cpu_ptr, gx_fifo_regs, stream, size);
    if (t->texture_elements == TEX_ELEMNTS_ST)
        t->elemets.T = (X32)read_stream(static_cpu_ptr, gx_fifo_regs, stream, size);
}

Vertex parse_vertex_from_stream(CPU *cpu, u32 VCD_HI, u32 VCD_LO, u32 VAT_A, u32 VAT_B, u32 VAT_C,
                                u32 CP_REGS[256], u8 **stream, PrimitiveType type, u16 count)
{
    Vertex out_v = {0};

    out_v.type = type;
    out_v.count = count;

    const u32 VAT[3] = {VAT_A, VAT_B, VAT_C};
    // Parse PosMat
    if (VCD_LO & 1) {
        out_v.pm.has_pos_mat_idx = true;
        out_v.pm.pos_mat_idx = (u8)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 1);
    }
    // Parse TexMat
    for (int i = 0; i < 8; i++) {
        if ((VCD_LO >> (i + 1)) & 1) {
            out_v.tm[i].has_tex_mat_idx = true;
            out_v.tm[i].tex_mat_idx = (u8)read_stream(static_cpu_ptr, gx_fifo_regs, stream, 1);
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
            idx = (u16)read_stream(static_cpu_ptr, gx_fifo_regs, stream, pos_type - 1);

            u8 *ram_stream =
                (u8 *)&cpu->bus->ram[(CP_REGS[0xA0] & 0x03FFFFFF) + idx * (CP_REGS[0xB0] & 0xFF)];
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
                    idx = (u16)read_stream(static_cpu_ptr, gx_fifo_regs, stream, norm_type - 1);

                    u8 *ram_stream =
                        (u8 *)&cpu->bus
                            ->ram[(CP_REGS[0xA1] & 0x03FFFFFF) + idx * (CP_REGS[0xB1] & 0xFF)];

                    vec[j].X = read_stream(static_cpu_ptr, gx_fifo_regs, &ram_stream, size);
                    vec[j].Y = read_stream(static_cpu_ptr, gx_fifo_regs, &ram_stream, size);
                    vec[j].Z = read_stream(static_cpu_ptr, gx_fifo_regs, &ram_stream, size);
                }
                break;

            } else {
                u16 idx = 0;
                idx = (u16)read_stream(static_cpu_ptr, gx_fifo_regs, stream, norm_type - 1);

                u8 *ram_stream =
                    (u8 *)&cpu->bus
                        ->ram[(CP_REGS[0xA1] & 0x03FFFFFF) + idx * (CP_REGS[0xB1] & 0xFF)];
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
            idx = (u16)read_stream(static_cpu_ptr, gx_fifo_regs, stream, color_type - 1);

            u8 *ram_stream =
                (u8 *)&cpu->bus
                    ->ram[(CP_REGS[0xA2 + i] & 0x03FFFFFF) + idx * (CP_REGS[0xB2 + i] & 0xFF)];

            _parse_color(VAT_A, &ram_stream, &out_v.color[i], i);
            break;
        }
    }
    // texture

    for (u8 i = 0; i < 8; i++) {
        const u8 texture_type = (VCD_HI >> (i * 2)) & 0x3;
        switch (texture_type) {
        case 0:
            out_v.texture[i].has_texture = false;
            break;
        case 1:
            _parse_tex(VAT, stream, &out_v.texture[i], i);
            break;
        case 2:
        case 3:
            u16 idx = (u16)read_stream(static_cpu_ptr, gx_fifo_regs, stream, texture_type - 1);

            u8 *ram_stream =
                (u8 *)&cpu->bus
                    ->ram[(CP_REGS[0xA4 + i] & 0x03FFFFFF) + idx * (CP_REGS[0xB4 + i] & 0xFF)];

            _parse_tex(VAT, &ram_stream, &out_v.texture[i], i);
            break;
        }
    }
    return out_v;
}
