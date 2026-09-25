#include "texture.h"
#include "bus/bus.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <assert.h>
#include <stdio.h>

void get_texture_unit_regs(TextureUnit *u, u8 tex_unit_num, const u32 *bp)
{

    const u8 idx = 0x80 | (tex_unit_num & 0x3) | ((tex_unit_num & 0x4) << 3);

    u32 *p = &u->mode0;
    for (int i = 0; i < 7; i++) {
        p[i] = bp[idx | (i << 2)];
    }
}

void load_texture_for_primitive(CPU *cpu, Vertex *v)
{
    const u32 *bp = get_bp_register_pointer();

    const u8 num_tex_gen = bp[0] & 0x7;

    const u8 num_tev_stages = ((bp[0] >> 10) & 0x7) + 1;
    if (!num_tex_gen) {
        // TODO: set no texture in vertex struct
        return;
    }

    for (int i = 0; i < num_tev_stages; i++) {
        const u32 tev_order = bp[0x28 + i];
        const bool is_even = i % 2 == 0;

        if (((tev_order >> TEV_ORDER_ENABLE_TEX(is_even)) & 1) == 0) {
            // tex nor enabled
            continue;
        }

        u8 tex_unit = (tev_order >> TEV_ORDER_TEX_MAP(is_even)) & 0x7;

        decode_texture(cpu, 0x0, 0x0, bp, tex_unit);
    }
}

void decode_texture(CPU *cpu, void *dest, const void *src, const u32 *bp, u8 tex_num)
{
    TextureUnit u = {0};
    get_texture_unit_regs(&u, tex_num, bp);

    const u32 ram_adr = (u.img3 & 0xFFFFFF);
    const u32 width = (u.img0 & 0x3FF) + 1;
    const u32 height = (((u.img0 >> 10) & 0x3FF) + 1);

    const u8 tex_format = ((u.img0 >> 20) & 0xF);

    if (tex_format == TEXTURE_FORMAT_I8) {
        i8_decode(&cpu->bus->ram[ram_adr << 5], width, height);
    } else if (tex_format == TEXTURE_FORMAT_I4) {
        i4_decode(&cpu->bus->ram[ram_adr << 5], width, height);
    } else if (tex_format == TEXTURE_FORMAT_RGBA8) {
        rgba8_decode(&cpu->bus->ram[ram_adr << 5], width, height);

    } else {

        printf("texture format   0x%02x\n", (u.img0 >> 20) & 0xF);
        assert(!"texutre format isnt implemented\n");
    }
}

void get_hash_from_bp_stat(const u32 *bp, char *dest)
{
}

void load_texture(const u8 tex_usage_bitmap, const char *hash)
{
}
