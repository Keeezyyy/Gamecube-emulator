#pragma once
#include "graphics/gpu/render/backend/software/texture/texture.h"

void opengl_encode_texture(CPU *cpu, TextureUnit *u, u32 *dest, u32 *upload_width,
                           u32 *upload_height);
