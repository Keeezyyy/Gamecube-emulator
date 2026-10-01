#include "registers.h"
#include "core/config/config.h"
#include "graphics/gpu/render/xf_types.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string.h>

static XF_Memory xf_mem;
static XF_Registers xf_reg;

static GLint xf_pos_loc, xf_norm_loc, proj_loc;

static GLuint xf_regs_buffer;
static GLuint xf_lights_buffer;

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

void opengl_write_to_bp(const u32 *bp, const u32 adr)
{
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
    xf_norm_loc = glGetUniformLocation(shader_program, "xf_norm");
    proj_loc = glGetUniformLocation(shader_program, "proj");

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
}
