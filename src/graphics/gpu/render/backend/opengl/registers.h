#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "bus/bus.h"
#include "core/config/config.h"
#include "cpu/cpu_types.h"

typedef struct ALIGNAS(16) {
    u32 mode0, mode1, img0, img1, img2, img3, lut;
} TevTextureUnit;

void opengl_write_to_bp(const u32 *bp, const u32 adr, const u32 value);
void opengl_write_to_cp(const u32 *bp, const u32 adr);
bool is_tex_unit_dirty(void);
void reupload_texture_units(CPU *cpu);

void opengl_write_to_xf_reg(const u32 reg_num, const u32 val);

u32 *opengl_get_xf_buffer(void);

void opengl_init_register(const GLuint shader_program);
