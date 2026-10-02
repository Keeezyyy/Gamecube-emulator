#include "registers.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/opengl/render.h"
#include "graphics/gpu/render/backend/opengl/texture/texture.h"
#include "graphics/gpu/render/backend/software/texture/texture.h"
#include "graphics/gpu/render/xf_types.h"

#include <stdio.h>
#include <string.h>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

static XF_Memory xf_mem;
static XF_Registers xf_reg;
static GLint xf_pos_loc, xf_norm_loc, proj_loc, xf_tex_loc, bp_reg_loc, tev_reg_start_loc,
    tev_konst_loc, tev_tmem_loc;

static GLuint xf_regs_buffer;
static GLuint xf_lights_buffer;
static GLuint tev_texture_unit_buffer;

static GLuint texture_buffer;

#define TMEM_TLUT_SIZE 0x80000
static GLuint tmem_buffer;
static GLuint tmem_texture;

static const u8 xf_reg_field_len[] = {1, 4, 1, 1, 1, 1, 1,  2, 2, 2, 2, 1,
                                      5, 1, 1, 6, 6, 1, 24, 1, 8, 8, 8};

#define XF_REG_COUNT (sizeof(XF_Registers) / sizeof(u32))
static GLintptr xf_reg_offset[XF_REG_COUNT];

static GLsizeiptr init_xf_reg_offsets(void)
{
    GLintptr offset = 0;
    u32 reg = 0;

    for (u32 f = 0; f < sizeof xf_reg_field_len; f++) {
        const u32 len = xf_reg_field_len[f];

        if (len == 1) {
            xf_reg_offset[reg++] = offset;
            offset += 4;
        } else {
            offset = (offset + 15) & ~(GLintptr)15;
            for (u32 i = 0; i < len; i++, offset += 16)
                xf_reg_offset[reg++] = offset;
        }
    }

    return (offset + 15) & ~(GLintptr)15;
}

static void load_new_proj(void)
{

    Mat4 out = {0};

    float p0 = xf_reg.projection[0];
    float p1 = xf_reg.projection[1];
    float p2 = xf_reg.projection[2];
    float p3 = xf_reg.projection[3];
    float p4 = xf_reg.projection[4];
    float p5 = xf_reg.projection[5];

    if (xf_reg.projection_type == 0) {
        // perspektivisch
        out.m[0][0] = p0;
        out.m[0][2] = p1;
        out.m[1][1] = p2;
        out.m[1][2] = p3;
        out.m[2][2] = p4;
        out.m[2][3] = p5;
        out.m[3][2] = -1.0f;
        out.m[3][3] = 0.0f;
    } else {
        // orthografisch
        out.m[0][0] = p0;
        out.m[0][3] = p1;
        out.m[1][1] = p2;
        out.m[1][3] = p3;
        out.m[2][2] = p4;
        out.m[2][3] = p5;
        out.m[3][3] = 1.0f;
    }

    glUniformMatrix4fv(proj_loc, 1, GL_TRUE, &out.m[0][0]);
}

// GPU:
// TODO: only upload on first primitive render and only upload dirty marked regs
void opengl_write_to_xf_reg(const u32 reg_num, const u32 val)
{
    if (reg_num < 0x680) {
        u32 *p = &xf_mem.matrices;
        p[reg_num] = val;

    } else if (reg_num >= 0x1000 && reg_num <= 0x1057) {

        u32 *p = &xf_reg.error;
        p[reg_num - 0x1000] = val;
        glBindBuffer(GL_UNIFORM_BUFFER, xf_regs_buffer);
        glBufferSubData(GL_UNIFORM_BUFFER, xf_reg_offset[reg_num - 0x1000], sizeof val, &val);
    }

    if (reg_num <= 0xFF) {
        // posMat
        glUniform4fv(xf_pos_loc, 64, &xf_mem.matrices[0][0]);
    } else if (reg_num >= 0x400 && reg_num <= 0x45F) {
        glUniform3fv(xf_norm_loc, 32, &xf_mem.normal_matrices[0][0]);
    } else if (reg_num >= 0x500 && reg_num <= 0x5FF) {
        glUniform4fv(xf_tex_loc, 64, &xf_mem.post_matrices[0][0]);
    } else if (reg_num >= 0x600 && reg_num <= 0x67F) {
        u32 std140[8][32] = {0};
        for (u32 l = 0; l < 8; l++) {
            const u32 *src = (const u32 *)&xf_mem.lights[l];
            for (u32 w = 0; w < 4; w++)
                std140[l][w * 4] = src[w];
            for (u32 v = 0; v < 4; v++)
                memcpy(&std140[l][16 + v * 4], &src[4 + v * 3], 3 * sizeof(u32));
        }
        glBindBuffer(GL_UNIFORM_BUFFER, xf_lights_buffer);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof std140, std140);
    }

    if (reg_num >= 0x1020 && reg_num <= 0x1026)
        load_new_proj();
}
static inline s16 sign_extend11(u32 v)
{
    s32 x = (s32)(v & 0x7FF);
    return (s16)((x & 0x400) ? x - 0x800 : x);
}

static u8 dirty_texture_unit_bitmap = 0;
static u8 dirty_texture_mem_bitmap = 0;

typedef struct {
    s16 rabg[4];
} s16Color;

static s16Color color_tev_reg_start[4];
static s16Color color_tev_regs[4];
static s16Color color_tev_konst[4];

enum { TEV_R = 0, TEV_G = 1, TEV_B = 2, TEV_A = 3 };

void opengl_write_to_bp(const u32 *bp, const u32 adr, const u32 value)
{
    glUniform1uiv(bp_reg_loc, 256, &bp[0]);

    if (adr >= 0x80 && adr <= 0xBB) {
        // texture unit registers

        const u8 unit = ((adr >> 3) & 0x4) | (adr & 0x3);
        if ((adr >= 0x80 && adr <= 0x87) || (adr >= 0xa0 && adr <= 0xa7)) {
            dirty_texture_unit_bitmap |= BIT(unit);
        } else {
            dirty_texture_unit_bitmap |= BIT(unit);
            dirty_texture_mem_bitmap |= BIT(unit);
        }
    } else if (adr >= 0xE0 && adr <= 0xe7) {

        const int idx = (adr - 0xE0) >> 1;
        s16Color *dst = (value & (1u << 23)) ? &color_tev_konst[idx] : &color_tev_reg_start[idx];

        const s16 low = sign_extend11(value);
        const s16 high = sign_extend11(value >> 12);

        if (adr & 1) {
            dst->rabg[TEV_B] = low;
            dst->rabg[TEV_G] = high;
        } else {
            dst->rabg[TEV_R] = low;
            dst->rabg[TEV_A] = high;
        }
        i32 buffer[16] = {0};

        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                buffer[(i * 4) + j] = color_tev_konst[i].rabg[j];
        glUniform1iv(tev_konst_loc, 16, buffer);

        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                buffer[(i * 4) + j] = color_tev_reg_start[i].rabg[j];
        glUniform1iv(tev_reg_start_loc, 16, buffer);
    }
}

void opengl_upload_tmem_texture(CPU *cpu, const u32 ram_adr, const u32 tmem_adr,
                                const u32 num_of_32_byte_blocks)
{

    const GLintptr offset = (GLintptr)(tmem_adr & 0x3FF) << 9;
    GLsizeiptr size = (GLsizeiptr)num_of_32_byte_blocks * 32;
    if (offset + size > TMEM_TLUT_SIZE)
        size = TMEM_TLUT_SIZE - offset;

    glBindBuffer(GL_TEXTURE_BUFFER, tmem_buffer);
    glBufferSubData(GL_TEXTURE_BUFFER, offset, size, &cpu->bus->ram[ram_adr << 5]);
}

bool is_tex_unit_dirty(void)
{
    return dirty_texture_unit_bitmap != 0 || dirty_texture_mem_bitmap != 0;
}

static void upload_layer(int layer, const uint32_t *pixels, const u32 width, const u32 height)
{
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_buffer);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 1024);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, width, height, 1, GL_RGBA_INTEGER,
                    GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

static u32 texel_buffer[1024 * 1024];
// GRAPHICS:
// TODO: use opengl native shaders and shader options
void reupload_texture_units(CPU *cpu)
{
    for (u8 i = 0; i < 8; i++) {
        if (!BIT_CHECK(dirty_texture_unit_bitmap, i) && !BIT_CHECK(dirty_texture_mem_bitmap, i))
            continue;

        TextureUnit u;

        get_texture_unit_regs(&u, i, get_bp_register_pointer());
        if (BIT_CHECK(dirty_texture_unit_bitmap, i)) {
            TevTextureUnit t =
                (TevTextureUnit){u.mode0, u.mode1, u.img0, u.img1, u.img2, u.img3, u.lut};

            const GLintptr stride = sizeof(TevTextureUnit);
            const GLintptr offset = i * stride;

            glBindBuffer(GL_UNIFORM_BUFFER, tev_texture_unit_buffer);
            glBufferSubData(GL_UNIFORM_BUFFER, offset, stride, &t);

            dirty_texture_unit_bitmap &= ~BIT(i);
        }
        if (BIT_CHECK(dirty_texture_mem_bitmap, i)) {

            u32 upload_width, upload_height;
            opengl_encode_texture(cpu, &u, &texel_buffer[0], &upload_width, &upload_height);
            upload_layer(i, &texel_buffer[0], upload_width, upload_height);

            // TODO: dont save textures in hash map or only in software renderer
            // free_texture(t);
            dirty_texture_mem_bitmap &= ~BIT(i);
        }
    }
}

void opengl_write_to_cp(const u32 *cp, const u32 adr)
{
}

static u32 buffer[0x1058];
u32 *opengl_get_xf_buffer(void)
{
    memcpy(&buffer, xf_mem.matrices, sizeof(XF_Memory));
    memcpy(&buffer[0x1000], &xf_reg, sizeof(XF_Registers));

    return buffer;
}

void opengl_init_register(const GLuint shader_program)
{
    xf_pos_loc = glGetUniformLocation(shader_program, "xf_pos");
    xf_tex_loc = glGetUniformLocation(shader_program, "xf_tex");
    xf_norm_loc = glGetUniformLocation(shader_program, "xf_norm");
    bp_reg_loc = glGetUniformLocation(shader_program, "bp_regs");
    proj_loc = glGetUniformLocation(shader_program, "proj");

    tev_reg_start_loc = glGetUniformLocation(shader_program, "tev_reg_start");
    tev_konst_loc = glGetUniformLocation(shader_program, "tev_konst");
    tev_tmem_loc = glGetUniformLocation(shader_program, "tev_tmem");

    static const u8 zero[XF_REG_COUNT * 16];
    const GLsizeiptr size = init_xf_reg_offsets();

    glGenBuffers(1, &xf_regs_buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, xf_regs_buffer);
    glBufferData(GL_UNIFORM_BUFFER, size, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, xf_regs_buffer);
    glUniformBlockBinding(shader_program,
                          glGetUniformBlockIndex(shader_program, "XFRegistersBlock"), 0);

    glGenBuffers(1, &xf_lights_buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, xf_lights_buffer);
    glBufferData(GL_UNIFORM_BUFFER, size, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, xf_lights_buffer);
    glUniformBlockBinding(shader_program, glGetUniformBlockIndex(shader_program, "XFLightsBlock"),
                          1);

    glGenBuffers(1, &tev_texture_unit_buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, tev_texture_unit_buffer);
    glBufferData(GL_UNIFORM_BUFFER, size, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, tev_texture_unit_buffer);
    glUniformBlockBinding(shader_program,
                          glGetUniformBlockIndex(shader_program, "TevTextureUnitBlock"), 2);

    // texture buffer binding
    glGenTextures(1, &texture_buffer);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_buffer);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8UI, 1024, 1024, 8, 0, GL_RGBA_INTEGER,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
    glUniform1i(glGetUniformLocation(shader_program, "textures_buffers"), 0);

    glGenBuffers(1, &tmem_buffer);
    glBindBuffer(GL_TEXTURE_BUFFER, tmem_buffer);
    glBufferData(GL_TEXTURE_BUFFER, TMEM_TLUT_SIZE, NULL, GL_DYNAMIC_DRAW);

    glGenTextures(1, &tmem_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, tmem_texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, tmem_buffer);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(tev_tmem_loc, 1);
}
