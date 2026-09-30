#version 410



struct XFRegisters {
    uint error;
    uint diag_state_clock[4];
    uint clip_disable;
    uint perf0;
    uint perf1;
    uint vtx_specs;
    uint num_channels;
    uint ambient_color[2];
    uint material_color[2];
    uint channel_color[2];
    uint channel_alpha[2];
    uint dual_tex;
    uint unknown_1013_1017[5];
    uint matrix_index_a;
    uint matrix_index_b;

    float viewport[6];
    float projection[6];

    uint projection_type;
    uint unknown_1027_103E[24];

    uint num_tex_gens;
    uint tex_mtx_info[8];
    uint unknown_1048_104F[8];
    uint post_mtx_info[8];
};

struct XFLight{
  uint unused[3];
  uint color;
  vec3 cosatt;
  vec3 distatt;
  vec3 pos;
  vec3 dir;
};

layout(std140) uniform XFRegistersBlock {
    XFRegisters xf_regs;
};

layout(std140) uniform XFLightsBlock {
    XFLight xf_lights[8];
};
