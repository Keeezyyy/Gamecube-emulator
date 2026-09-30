#include "vertex_conversion.h"
#include "core/config/config.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string.h>

static float decode_component(X32 raw, FormatDataType t, u8 frac)
{
    const float scale = 1.0f / (float)(1u << frac);
    switch (t) {
    case DATA_TYPE_U8:
        return (float)(u8)raw * scale;
    case DATA_TYPE_S8:
        return (float)(s8)raw * scale;
    case DATA_TYPE_U16:
        return (float)(u16)raw * scale;
    case DATA_TYPE_S16:
        return (float)(s16)raw * scale;
    case DATA_TYPE_F32: {
        float f;
        memcpy(&f, &raw, sizeof f);
        return f;
    }
    default:
        return 0.0f;
    }
}

static u8 normal_frac(FormatDataType t)
{
    switch (t) {
    case DATA_TYPE_U8:
    case DATA_TYPE_S8:
        return 6;
    case DATA_TYPE_U16:
    case DATA_TYPE_S16:
        return 14;
    default:
        return 0;
    }
}

static void decode_vec3(float out[3], const Vec3 *in, FormatDataType t, u8 frac)
{
    out[0] = decode_component(in->X, t, frac);
    out[1] = decode_component(in->Y, t, frac);
    out[2] = decode_component(in->Z, t, frac);
}

GpuVertex vertex_to_gpu(const Vertex *v)
{
    GpuVertex g;
    memset(&g, 0, sizeof g);
    u32 flags = 0;

    const VertexPosition *p = &v->pos;
    g.pos[0] = decode_component(p->vec.X, p->position_data_type, p->frac);
    g.pos[1] = decode_component(p->vec.Y, p->position_data_type, p->frac);
    if (p->position_elements == POS_ELEMNTS_XYZ) {
        g.pos[2] = decode_component(p->vec.Z, p->position_data_type, p->frac);
        flags |= GPUV_FLAG_POS_3D;
    }

    const VertexNormal *n = &v->norm;
    if (n->has_normal) {
        u8 nf = normal_frac(n->normal_data_type);
        decode_vec3(g.normal, &n->N, n->normal_data_type, nf);
        flags |= GPUV_FLAG_NORMAL;
        if (n->normal_elements == NORMAL_ELEMENTS_NBT) {
            decode_vec3(g.binormal, &n->B, n->normal_data_type, nf);
            decode_vec3(g.tangent, &n->T, n->normal_data_type, nf);
            flags |= GPUV_FLAG_NBT;
        }
    }

    for (int c = 0; c < 2; c++) {
        if (v->color[c].has_color) {
            memcpy(g.color[c], v->color[c].rgba, 4);
            flags |= (c == 0) ? GPUV_FLAG_COLOR0 : GPUV_FLAG_COLOR1;
        } else {
            memset(g.color[c], 0xFF, 4);
        }
    }

    for (int i = 0; i < 8; i++) {
        const VertexTexture *t = &v->texture[i];
        if (t->has_texture) {
            g.tex[i][0] = decode_component(t->elemets.S, t->texture_data_type, t->frac);
            g.tex[i][1] = (t->texture_elements == TEX_ELEMNTS_ST)
                              ? decode_component(t->elemets.T, t->texture_data_type, t->frac)
                              : 0.0f;
            flags |= 1u << (GPUV_TEX_SHIFT + i);
        }
        if (v->tm[i].has_tex_mat_idx) {
            flags |= 1u << (GPUV_TEXMAT_SHIFT + i);
            g.tex_mat_idx[i / 4] |= (u32)v->tm[i].tex_mat_idx << (8 * (i % 4));
        }
    }

    memcpy(g.pos_mat, v->pm.mat, sizeof g.pos_mat);
    memcpy(g.norm_mat, v->norm.normal_matrix.m, sizeof(Float3) * 3);
    flags |= (u32)v->pm.posMatId << GPUV_POSMAT_SHIFT;

    g.flags = flags;
    return g;
}

#define OFS(field) ((void *)offsetof(GpuVertex, field))

void gpu_vertex_setup_attribs(void)
{
    const GLsizei stride = sizeof(GpuVertex);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, OFS(pos));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, OFS(normal));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, OFS(binormal));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, OFS(tangent));

    glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, OFS(color[0]));
    glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, OFS(color[1]));

    for (int i = 0; i < 4; i++)
        glVertexAttribPointer(6 + i, 4, GL_FLOAT, GL_FALSE, stride,
                              (void *)(offsetof(GpuVertex, tex) + i * 4 * sizeof(float)));

    for (int i = 0; i < 3; i++)
        glVertexAttribPointer(10 + i, 4, GL_FLOAT, GL_FALSE, stride,
                              (void *)(offsetof(GpuVertex, pos_mat) + i * sizeof(Float4)));

    glVertexAttribIPointer(13, 1, GL_UNSIGNED_INT, stride, OFS(flags));
    glVertexAttribIPointer(14, 2, GL_UNSIGNED_INT, stride, OFS(tex_mat_idx));
    for (int i = 0; i < 3; i++)
        glVertexAttribPointer(15 + i, 4, GL_FLOAT, GL_FALSE, stride,
                              (void *)(offsetof(GpuVertex, norm_mat) + i * sizeof(Float3)));

    for (int i = 0; i <= 14; i++)
        glEnableVertexAttribArray(i);
}
