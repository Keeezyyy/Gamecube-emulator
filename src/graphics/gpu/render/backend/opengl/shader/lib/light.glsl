#version 410

#include "types.glsl"


uint reg_to_rgba(uint reg)
{
    uint val = 0u;

    val |= (reg >> 24u);
    val |= ((reg >> 16u) & 0xFFu) << 8u;
    val |= ((reg >> 8u) & 0xFFu) << 16u;
    val |= (reg & 0xFFu) << 24u;

    return uint(val);
}


uint convert_normilized_vec4_to_rgba(vec4 v){
  uint packed =
        (uint(round(clamp(v.r, 0.0, 1.0) * 255.0))      ) |
        (uint(round(clamp(v.g, 0.0, 1.0) * 255.0)) <<  8) |
        (uint(round(clamp(v.b, 0.0, 1.0) * 255.0)) << 16) |
        (uint(round(clamp(v.a, 0.0, 1.0) * 255.0)) << 24);

  return uint(packed);
}


uint add_lights(uint ctrl, vec3 eye, mat3x3 nbt, vec4 lit){
    uint mask = ((ctrl >> 2u) & 0xFu) | (((ctrl >> 11u) & 0xFu) << 4u);
    uint diff_fn = (ctrl >> 7) & 3u;
    uint attn_fn = (ctrl >> 9u) & 3u;

    for (int l = 0; l < 8; l++) {
        if (((mask >> l) & 1u) == 0){
            continue;
        }
        
        vec3 ldir = xf_lights[l].pos - eye;
        

        float attn;
        switch (attn_fn) {
          case 1:
            ldir = normalize(ldir);
            attn = dot(ldir, nbt[0]) >= 0.0f  ? max(0.0f, dot(ldir, nbt[0])) : 0.0f;

            vec3 A = vec3(1.0f, attn, attn * attn);
            vec3 K = xf_lights[l].distatt;
            if(diff_fn != 0)
              K = normalize(K);

            float dist = dot(A, K);
            attn = dist != 0.0f ? max(0.0f, dot(A, xf_lights[l].cosatt)) / dist : 0.0f;
            break;
        }

    }
    return uint(0);
}

uint calc_light(vec3 eye, mat3x3 nbt, uint color, uint i){
  uint out_color = 0;

  uint num_of_chans = xf_regs.num_channels & 3u;

  uint mat_reg = reg_to_rgba(xf_regs.material_color[i]);
  uint amb_reg = reg_to_rgba(xf_regs.ambient_color[i]);

  uint ctrls[2];
  ctrls[0] = xf_regs.channel_color[i];
  ctrls[1] = xf_regs.channel_alpha[i];


  for (int k = 0; k < 2; k++) {
    uint ctrl = ctrls[k];
    uint first = k == 0 ? 0 : 3;
    uint last = k == 0 ? 3 : 4;

    uint mask = k == 0 ? 0x00FFFFFFu : 0xFF000000u;

    uint mat = (ctrl & 1u)==1 ? color : mat_reg;


    if (((ctrl >> 1u) & 1u) == 0) {
        for (uint c = first; c < last; c++)
          out_color |= mat & mask;
        continue;
    }

    uint amb = (ctrl & 1u)==1 ? color : amb_reg;

    vec4 lit = vec4(uvec4(amb & 0xFFu, (amb >> 8) & 0xFFu, (amb >> 16) & 0xFFu, (amb >> 24) & 0xFFu));

    


  }

  return uint(0);
}

