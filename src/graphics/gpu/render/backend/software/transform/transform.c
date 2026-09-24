#include "transform.h"
#include "core/config/config.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <cblas.h>
#include <assert.h>
#include <math.h>
#include <string.h>

static XF_Memory xf_mem;
static XF_Registers xf_reg;

static u32 buffer[0x1058];
const u32 *software_backend_get_xf_buffer(void)
{
    memcpy(buffer, xf_mem.matrices, sizeof(XF_Memory));
    memcpy(&buffer[0x1000], &xf_reg, sizeof(XF_Registers));

    return buffer;
}

void software_backend_write_to_xf_reg(const u32 reg_num, const u32 val)
{
    if (reg_num <= 0x1000) {
        u32 *p = &xf_mem.matrices;
        p[reg_num] = val;

        assert(reg_num <= 0x067F);
    } else {

        u32 *p = &xf_reg.error;
        p[reg_num - 0x1000] = val;

        assert(reg_num <= 0x1058);
    }
}

static Mat4 build_projection(void)
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
    return out;
}
static Mat4 build_viewport(void)
{
    float sx = xf_reg.viewport[0];
    float sy = xf_reg.viewport[1];
    float sz = xf_reg.viewport[2];

    float cx = xf_reg.viewport[3];
    float cy = xf_reg.viewport[4];
    float farZ = xf_reg.viewport[5];
    Mat4 m = {0};

    m.m[0][0] = sx;
    m.m[0][3] = cx;

    m.m[1][1] = sy;
    m.m[1][3] = cy;

    m.m[2][2] = sz;
    m.m[2][3] = farZ;

    m.m[3][3] = 1.0f;

    return m;
}

static Float4 read_position(const Vertex *v)
{
    Float4 pos = {0.0f, 0.0f, 0.0f, 1.0f};

    const u32 *raw = (const u32 *)&v->pos.vec;
    const int count = 2 + (int)v->pos.position_elements;

    for (int i = 0; i < count; i++) {
        switch (v->pos.position_data_type) {
        case DATA_TYPE_U8:
            pos.m[i] = (f32)(u8)raw[i];
            break;
        case DATA_TYPE_S8:
            pos.m[i] = (f32)(s8)raw[i];
            break;
        case DATA_TYPE_U16:
            pos.m[i] = (f32)(u16)raw[i];
            break;
        case DATA_TYPE_S16:
            pos.m[i] = (f32)(s16)raw[i];
            break;
        case DATA_TYPE_F32: {
            f32 f;
            memcpy(&f, &raw[i], sizeof f);
            pos.m[i] = f;
            break;
        }
        default:
            assert(!"unbekannter Positions-Datentyp");
            break;
        }
    }
    if (v->pos.frac != 0 && v->pos.position_data_type != DATA_TYPE_F32) {
        for (int i = 0; i < (2 + (u8)v->pos.position_elements); i++) {
            f32 *cord = &pos.m[0];

            cord[i] /= powf(2.0f, (f32)v->pos.frac);
        }
    }

    return pos;
}

static void calc_pos(Float4 *out, const Vertex *v)
{

    Float4 pos = read_position(v);

    f32 posMat[4][4] = {0};
    memcpy(posMat, v->pm.mat, sizeof(f32) * 3 * 4);
    posMat[3][3] = 1.0f;

    Mat4 proj = build_projection();
    f32 mvp[4][4];
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, 4, 4, 4, 1.0f, &proj.m[0][0], 4,
                &posMat[0][0], 4, 0.0f, &mvp[0][0], 4);

    Float4 clip;
    cblas_sgemv(CblasRowMajor, CblasNoTrans, 4, 4, 1.0f, &mvp[0][0], 4, pos.m, 1, 0.0f, clip.m, 1);

    const f32 w = clip.m[3];

    if (w <= 0.0f) {
        assert(!"implement clipping or ");
    }

    const f32 inv_w = 1.0f / w;
    Float4 ndc = {
        clip.m[0] * inv_w,
        clip.m[1] * inv_w,
        clip.m[2] * inv_w,
        1.0f,
    };

    Mat4 view = build_viewport();
    cblas_sgemv(CblasRowMajor, CblasNoTrans, 4, 4, 1.0f, &view.m[0][0], 4, ndc.m, 1, 0.0f, out->m,
                1);

    out->m[3] = inv_w;
}
static void calc_nomral(const Vertex *v, Float3 *NBT)
{

    for (u32 j = 0; j < ((v->norm.normal_elements * 2) + 1); j++) {

        Float3 val = {0.0, 0.0, 0.0};

        memcpy(val.m, &(((Float3 *)&v->norm.N)[j]), sizeof(Float3));

        cblas_sgemv(CblasRowMajor, CblasNoTrans, 3, 3, 1.0f, (u32 *)v->norm.normal_matrix.m, 3,
                    val.m, 1, 0.0f, &NBT[j].m, 1);
    }
}

bool transform_vertex(const Vertex *v)
{
    Float4 out;
    calc_pos(&out, v);

    Float3 NBT[3];
    calc_nomral(v, NBT);

    return true;
}
