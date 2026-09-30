#include "registers.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string.h>

static XF_Memory xf_mem;
static XF_Registers xf_reg;

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

    glUniformMatrix4fv(projection_loc, 1, GL_TRUE, &out.m[0][0]);
}

void opengl_write_to_xf_reg(const u32 reg_num, const u32 val)
{
    if (reg_num < 0x680) {
        u32 *p = &xf_mem.matrices;
        p[reg_num] = val;

    } else if (reg_num >= 0x1000 && reg_num <= 0x1057) {

        u32 *p = &xf_reg.error;
        p[reg_num - 0x1000] = val;
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
