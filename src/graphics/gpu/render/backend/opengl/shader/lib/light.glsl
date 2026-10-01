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

vec4 pack_light(uint packed)
{
    vec4 v = vec4(
        float( packed        & 0xFFu),
        float((packed >>  8) & 0xFFu),
        float((packed >> 16) & 0xFFu),
        float((packed >> 24) & 0xFFu)
    ) / 255.0;
    return vec4(v);
}


vec4 add_lights(uint ctrl, vec3 eye, mat3x3 nbt, vec4 lit){
    vec4 lit_out  = lit;
    
    
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
          case 1u:
            ldir = normalize(ldir);
            attn = dot(ldir, nbt[0]) >= 0.0f  ? max(0.0f, dot(xf_lights[l].dir, nbt[0])) : 0.0f;

            vec3 A = vec3(1.0f, attn, attn * attn);
            vec3 K = xf_lights[l].distatt;
            if(diff_fn != 0)
              K = normalize(K);

            float dist = dot(A, K);
            attn = dist != 0.0f ? max(0.0f, dot(A, xf_lights[l].cosatt)) / dist : 0.0f;
            break;
          case 3u:
            float d = length(ldir);
            if(d > 0.0f)
              ldir = (1.0f/d) * ldir;

            float c = max(0.0f, dot(ldir, xf_lights[l].dir));
            float cos_attn = xf_lights[l].cosatt[0] + xf_lights[l].cosatt[1] * c + xf_lights[l].cosatt[2] * c *c;
            dist = xf_lights[l].distatt[0] + xf_lights[l].distatt[1] * d + xf_lights[l].distatt[2] * d *d;

            attn = dist != 0.0f ? (max(0.0f, cos_attn) / dist) : (0.0f);
            break;
          default: 
            if(length(ldir)>0.0f)
              ldir = normalize(ldir);
            else
              ldir = nbt[0];
            attn = 1.0f;
            break;
        }

        float diff = 1.0f;
        if(diff_fn != 0){
          diff = dot(ldir, nbt[0]);
          if(diff_fn !=1)
            diff = max(0.0f, diff);
        }

        
        uint col = reg_to_rgba(xf_lights[l].color);
        for(uint i = 0; i<4;i++){
            lit_out[i] += float((col>>(8*i))&0xFFu) * attn * diff;
        }

    }
    return lit_out;
}

uint calc_light(vec3 eye, mat3x3 nbt, uint color, uint i){
  uint out_color = 0;

  uint num_of_chans = xf_regs.num_channels & 3u;
  if (i >= num_of_chans)
    return 0u;

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

    uint amb = ((ctrl >> 6u) & 1u)==1 ? color : amb_reg;

    vec4 lit = vec4(uvec4(amb & 0xFFu, (amb >> 8) & 0xFFu, (amb >> 16) & 0xFFu, (amb >> 24) & 0xFFu));

    vec4 new_lit = add_lights(ctrl, eye, nbt, lit);
    

    for (uint c = first; c < last; c++) {
      uint l = uint(clamp(new_lit[c], 0.0f, 255.0f));
      out_color |= ((((mat >>(c*8u)) & 0xFFu) * (l + (l>>7)))>>8) << c*8;
    }


  }

  return uint(out_color);
}

