#include "transform.h"
#include "core/config/config.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include <cblas.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
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

const XF_Registers *get_xf_regs(void)
{
    return &xf_reg;
}
static void normalize3(f32 *vec)
{
    const f32 len = cblas_snrm2(3, vec, 1);
    if (len > 0.0f)
        cblas_sscal(3, 1.0f / len, vec, 1);
}

void software_backend_write_to_xf_reg(const u32 reg_num, const u32 val)
{
    if (reg_num < 0x680) {
        u32 *p = &xf_mem.matrices;
        p[reg_num] = val;

    } else if (reg_num >= 0x1000 && reg_num <= 0x1057) {

        u32 *p = &xf_reg.error;
        p[reg_num - 0x1000] = val;

        // assert(reg_num <= 0x1058);
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

static Float4 read_texcoords(const Vertex *v, u32 idx)
{
    Float4 out = {0.0, 0.0, 1.0f, 1.0f};

    assert(v->texture[idx].has_texture);

    const u32 *raw = (const u32 *)&v->texture[idx].elemets;
    const int count = 1 + (int)v->texture[idx].texture_elements;

    for (int i = 0; i < count; i++) {
        switch (v->texture[idx].texture_data_type) {
        case DATA_TYPE_U8:
            out.m[i] = (f32)(u8)raw[i];
            break;
        case DATA_TYPE_S8:
            out.m[i] = (f32)(s8)raw[i];
            break;
        case DATA_TYPE_U16:
            out.m[i] = (f32)(u16)raw[i];
            break;
        case DATA_TYPE_S16:
            out.m[i] = (f32)(s16)raw[i];
            break;
        case DATA_TYPE_F32: {
            f32 f;
            memcpy(&f, &raw[i], sizeof f);
            out.m[i] = f;
            break;
        }
        default:
            assert(!"unbekannter Positions-Datentyp");
            break;
        }
    }
    if (v->texture[idx].frac != 0 && v->texture[idx].texture_data_type != DATA_TYPE_F32) {
        for (int i = 0; i < (1 + v->texture[idx].texture_elements); i++) {
            f32 *cord = &out.m[0];

            cord[i] /= powf(2.0f, (f32)v->texture[idx].frac);
        }
    }
    return out;
}

static Float3 read_normal(const Vertex *v, const u32 j)
{
    Float3 norm = {0.0f, 0.0f, 0.0f};

    if (!v->norm.has_normal)
        return norm;

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

    out->m[0] = clip.m[0];
    out->m[1] = clip.m[1];
    out->m[2] = clip.m[2] * (1.0f - 1e-7f);
    out->m[3] = clip.m[3];
}
static void calc_nomral(const Vertex *v, Float3 *NBT)
{

    for (u32 j = 0; j < ((v->norm.normal_elements * 2) + 1); j++) {

        Float3 val = read_normal(v, j);

        cblas_sgemv(CblasRowMajor, CblasNoTrans, 3, 3, 1.0f, (u32 *)v->norm.normal_matrix.m, 3,
                    val.m, 1, 0.0f, NBT[j].m, 1);
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
    memcpy(&c, &be, sizeof(RGBA));

    return c;
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
            lit[c] += (f32)((const u8 *)&col)[c] * attn * diff;
    }
}

static void calc_light(const Vertex *v, const Float3 *eye, const Float3 *N, RGBA out[2],
                       XF_Light *lights)

{
    const u32 num_chans = xf_reg.num_channels & 3;

    for (u32 i = 0; i < 2 && i < num_chans; i++) {
        const RGBA mat_reg = reg_to_rgba(xf_reg.material_color[i]);
        const RGBA amb_reg = reg_to_rgba(xf_reg.ambient_color[i]);
        const u8 *vtx = v->color[i].rgba;

        const u32 ctrls[2] = {xf_reg.channel_color[i], xf_reg.channel_alpha[i]};

        const u32 mask = ((ctrls[0] >> 2) & 0xF) | (((ctrls[0] >> 11) & 0xF) << 4);
        if (mask)
            lights[i] = xf_mem.lights[__builtin_ctz(mask)];

        for (int k = 0; k < 2; k++) {
            const u32 ctrl = ctrls[k];
            const int first = k == 0 ? 0 : 3;
            const int last = k == 0 ? 3 : 4;

            const u8 *mat = (ctrl & 1) ? vtx : (const u8 *)&mat_reg;

            if (!((ctrl >> 1) & 1)) {
                for (int c = first; c < last; c++)
                    ((u8 *)&out[i])[c] = mat[c];
                continue;
            }

            const u8 *amb = ((ctrl >> 6) & 1) ? vtx : (const u8 *)&amb_reg;

            f32 lit[4];
            for (int c = 0; c < 4; c++)
                lit[c] = (f32)amb[c];

            add_lights(ctrl, eye, N, lit);

            for (int c = first; c < last; c++) {
                const u32 l = (u32)fminf(fmaxf(lit[c], 0.0f), 255.0f);
                ((u8 *)&out[i])[c] = (u8)((mat[c] * (l + (l >> 7))) >> 8);
            }
        }
    }
}

static Float4 get_texcoord_input(const Vertex *v, const u32 ctrl, const u8 idx)
{
    const u8 row = (ctrl >> 7) & 0x1F;
    Float4 in = {0.0f, 0.0f, 1.0f, 1.0f};

    switch (row) {
    case 0:
        in = read_position(v);
        break;
    case 1:
    case 3:
    case 4: {
        const Float3 n = read_normal(v, row == 1 ? 0 : row - 2);
        memcpy(in.m, n.m, sizeof(Float3));
        break;
    }
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        in = read_texcoords(v, row - 5);
        break;
    default:
        assert(!"ungueltige Texgen-Quelle");
        break;
    }

    if (!((ctrl >> 2) & 1))
        in.m[2] = 1.0f;
    in.m[3] = 1.0f;

    return in;
}

static void calc_tex_gen(CPU *cpu, const Vertex *v, const Float3 *eye,
                         const Float3 *NBT, const RGBA *colors, Float3 *tex_out)
{
    const u8 num_of_tex_gens = xf_reg.num_tex_gens;

    for (int i = 0; i < num_of_tex_gens; i++) {
        const u32 ctrl = xf_reg.tex_mtx_info[i];
        const u8 type = (ctrl >> 4) & 0x7;
        const bool is_STQ = (ctrl >> 1) & 1;

        Float3 out = {0};

        switch (type) {
        case 0: {
            // regualar matrix
            u8 matIdx = v->tm[i].tex_mat_idx;
            if (!v->tm[i].has_tex_mat_idx) {
                if (i <= 3)
                    matIdx = (xf_reg.matrix_index_a >> (6 + i * 6)) & 0x3F;
                if (i > 3)
                    matIdx = (xf_reg.matrix_index_b >> ((i - 4) * 6)) & 0x3F;
            }

            Float4 texCoord = get_texcoord_input(v, ctrl, i);
            Float4 mat[3] = {0};
            if (is_STQ) {
                memcpy(mat, xf_mem.matrices[matIdx], sizeof(Float4) * 3);
                cblas_sgemv(CblasRowMajor, CblasNoTrans, 3, 4, 1.0f, &mat[0].m[0], 4, texCoord.m, 1,
                            0.0f, out.m, 1);

            } else {
                memcpy(mat, xf_mem.matrices[matIdx], sizeof(Float4) * 2);
                cblas_sgemv(CblasRowMajor, CblasNoTrans, 2, 4, 1.0f, &mat[0].m[0], 4, texCoord.m, 1,
                            0.0f, out.m, 1);

                out.m[2] = 1.0f;
            }
            break;
        }
        case 1: {
            // EMboss
            const u8 lightIdx = (ctrl >> 15) & 0x7;
            const u8 src = (ctrl >> 12) & 0x7;
            const XF_Light *light = &xf_mem.lights[lightIdx];

            Float3 L = {{
                light->pos[0] - eye->m[0],
                light->pos[1] - eye->m[1],
                light->pos[2] - eye->m[2],
            }};
            normalize3(L.m);

            out.m[0] = tex_out[src].m[0] + cblas_sdot(3, L.m, 1, NBT[1].m, 1);
            out.m[1] = tex_out[src].m[1] + cblas_sdot(3, L.m, 1, NBT[2].m, 1);
            out.m[2] = 1.0f;
            break;
        }
        case 2:
            out.m[0] = ((f32)colors[0].r) / 255.0f;
            out.m[1] = ((f32)colors[0].g) / 255.0f;
            out.m[2] = 1.0f;
            break;
        case 3:
            out.m[0] = ((f32)colors[1].r) / 255.0f;
            out.m[1] = ((f32)colors[1].g) / 255.0f;
            out.m[2] = 1.0f;

            break;
        }

        if (type == 0 && (xf_reg.dual_tex & 1)) {
            const u32 post = xf_reg.post_mtx_info[i];
            if ((post >> 8) & 1)
                normalize3(out.m);

            const Float4 in = {out.m[0], out.m[1], out.m[2], 1.0f};
            cblas_sgemv(CblasRowMajor, CblasNoTrans, 3, 4, 1.0f, xf_mem.post_matrices[post & 0x3F],
                        4, in.m, 1, 0.0f, out.m, 1);
        }

        if (out.m[2] == 0.0f) {
            out.m[0] = fminf(fmaxf(out.m[0] / 2.0f, -1.0f), 1.0f);
            out.m[1] = fminf(fmaxf(out.m[1] / 2.0f, -1.0f), 1.0f);
        }

        tex_out[i] = out;
    }
}
XFOutput transform_vertex(CPU *cpu, const Vertex *v)
{
    XFOutput output = {0};

    calc_pos(&output.pos, v);

    calc_nomral(v, output.NBT);

    output.eye = calc_eye_pos(v);

    calc_light(v, &output.eye, output.NBT, output.colors, output.lights);

    calc_tex_gen(cpu, v, &output.eye, output.NBT, output.colors, output.tex);

    return output;
}
