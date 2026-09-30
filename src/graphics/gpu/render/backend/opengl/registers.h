#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "graphics/gpu/render/xf_types.h"

void opengl_write_to_bp(const u32 *bp, const u32 adr);
void opengl_write_to_cp(const u32 *bp, const u32 adr);

void opengl_write_to_xf_reg(const u32 reg_num, const u32 val);

u32 *opengl_get_xf_buffer(void);

void opengl_init_register(const GLuint shader_program);
