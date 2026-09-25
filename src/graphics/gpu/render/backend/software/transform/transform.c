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

static Float3 read_normal(const Vertex *v, const u32 j)
{
    Float3 norm = {0.0f, 0.0f, 0.0f};

    const u32 *raw = &(&v->norm.N)[j].X;

    for (int i = 0; i < 3; i++) {
        switch (v->norm.normal_data_type) {
        case DATA_TYPE_S8:
            norm.m[i] = (f32)(s8)raw[i] / 64.0f;
            break;
        case DATA_TYPE_S16:
            norm.m[i] = (f32)(s16)raw[i] / 16384.0f;
            break;
        case DATA_TYPE_F32: {
            f32 f;
            memcpy(&f, &raw[i], sizeof f);
            norm.m[i] = f;
            break;
        }
        default:
            assert(!"unbekannter Positions-Datentyp");
            break;
        }
    }

    return norm;
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

        Float3 val = read_normal(v, j);

        cblas_sgemv(CblasRowMajor, CblasNoTrans, 3, 3, 1.0f, (u32 *)v->norm.normal_matrix.m, 3,
                    val.m, 1, 0.0f, &NBT[j].m, 1);
    }

    const f32 len = cblas_snrm2(3, NBT[0].m, 1);
    if (len > 0.0f)
        cblas_sscal(3, 1.0f / len, NBT[0].m, 1);
}

static Float3 calc_eye_pos(const Vertex *v)
{
    Float4 pos = read_position(v);
    Float3 eye;

    cblas_sgemv(CblasRowMajor, CblasNoTrans, 3, 4, 1.0f, v->pm.mat[0].m, 4, pos.m, 1, 0.0f, eye.m,
                1);

    return eye;
}

static RGBA reg_to_rgba(const u32 reg)
{
    RGBA c;
    const u32 be = __builtin_bswap32(reg);
    memcpy(c.rgba, &be, sizeof(RGBA));

    return c;
}

static void normalize3(f32 *vec)
{
    const f32 len = cblas_snrm2(3, vec, 1);
    if (len > 0.0f)
        cblas_sscal(3, 1.0f / len, vec, 1);
}

static void add_lights(const u32 ctrl, const Float3 *eye, const Float3 *N, f32 lit[4])
{
    const u32 mask = ((ctrl >> 2) & 0xF) | (((ctrl >> 11) & 0xF) << 4);
    const u32 diff_fn = (ctrl >> 7) & 3;
    const u32 attn_fn = (ctrl >> 9) & 3;

    for (int l = 0; l < 8; l++) {
        if (!((mask >> l) & 1))
            continue;

        const XF_Light *light = &xf_mem.lights[l];

        Float3 ldir = {{
            light->pos[0] - eye->m[0],
            light->pos[1] - eye->m[1],
            light->pos[2] - eye->m[2],
        }};
        f32 attn;

        switch (attn_fn) {
        case 1: {
            normalize3(ldir.m);

            attn = cblas_sdot(3, ldir.m, 1, N->m, 1) >= 0.0f
                       ? fmaxf(0.0f, cblas_sdot(3, light->dir, 1, N->m, 1))
                       : 0.0f;

            const f32 A[3] = {1.0f, attn, attn * attn};
            f32 k[3];
            memcpy(k, light->distatt, sizeof k);
            if (diff_fn != 0)
                normalize3(k);

            const f32 dist = cblas_sdot(3, A, 1, k, 1);
            attn = dist != 0.0f ? fmaxf(0.0f, cblas_sdot(3, A, 1, light->cosatt, 1)) / dist : 0.0f;
            break;
        }
        case 3: {
            const f32 d = cblas_snrm2(3, ldir.m, 1);
            if (d > 0.0f)
                cblas_sscal(3, 1.0f / d, ldir.m, 1);

            const f32 c = fmaxf(0.0f, cblas_sdot(3, ldir.m, 1, light->dir, 1));
            const f32 cos_attn = light->cosatt[0] + light->cosatt[1] * c + light->cosatt[2] * c * c;
            const f32 dist = light->distatt[0] + light->distatt[1] * d + light->distatt[2] * d * d;

            attn = dist != 0.0f ? fmaxf(0.0f, cos_attn) / dist : 0.0f;
            break;
        }
        default:
            if (cblas_snrm2(3, ldir.m, 1) > 0.0f)
                normalize3(ldir.m);
            else
                ldir = *N;

            attn = 1.0f;
            break;
        }

        f32 diff = 1.0f;
        if (diff_fn != 0) {
            diff = cblas_sdot(3, ldir.m, 1, N->m, 1);
            if (diff_fn != 1)
                diff = fmaxf(0.0f, diff);
        }

        const RGBA col = reg_to_rgba(light->color);
        for (int c = 0; c < 4; c++)
            lit[c] += (f32)col.rgba[c] * attn * diff;
    }
}

static void calc_light(const Vertex *v, const Float3 *eye, const Float3 *N, RGBA out[2])
{
    const u32 num_chans = xf_reg.num_channels & 3;

    for (u32 i = 0; i < 2 && i < num_chans; i++) {
        const RGBA mat_reg = reg_to_rgba(xf_reg.material_color[i]);
        const RGBA amb_reg = reg_to_rgba(xf_reg.ambient_color[i]);
        const u8 *vtx = v->color[i].rgba;

        const u32 ctrls[2] = {xf_reg.channel_color[i], xf_reg.channel_alpha[i]};

        for (int k = 0; k < 2; k++) {
            const u32 ctrl = ctrls[k];
            const int first = k == 0 ? 0 : 3;
            const int last = k == 0 ? 3 : 4;

            const u8 *mat = (ctrl & 1) ? vtx : mat_reg.rgba;

            if (!((ctrl >> 1) & 1)) {
                for (int c = first; c < last; c++)
                    out[i].rgba[c] = mat[c];
                continue;
            }

            const u8 *amb = ((ctrl >> 6) & 1) ? vtx : amb_reg.rgba;

            f32 lit[4];
            for (int c = 0; c < 4; c++)
                lit[c] = (f32)amb[c];

            add_lights(ctrl, eye, N, lit);

            for (int c = first; c < last; c++) {
                const u32 l = (u32)fminf(fmaxf(lit[c], 0.0f), 255.0f);
                out[i].rgba[c] = (u8)((mat[c] * (l + (l >> 7))) >> 8);
            }
        }
    }
}

bool transform_vertex(const Vertex *v)
{
    Float4 out;
    calc_pos(&out, v);

    Float3 NBT[3];
    calc_nomral(v, NBT);

    const Float3 eye = calc_eye_pos(v);

    RGBA colors[2] = {0};
    calc_light(v, &eye, &NBT[0], colors);

    return true;
}
