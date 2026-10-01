#version 410

#include "types.glsl"

uint texMatIdx(uvec2 tex_mat_idx, uint i) { return (tex_mat_idx[i / 4u] >> (8u * (i % 4u))) & 0xFFu; }

vec4 get_texcoord_input(vec3 pos, mat3x3 raw_nbt, vec4[4] tex_coords, uint ctrl){
  uint row = (ctrl >> 7u) & 0x1Fu;
  vec4 tex_in = vec4(0.0f, 0.0f, 1.0f, 1.0f);

  if (row == 0u) {
    tex_in.xyz = pos;
  } else if (row == 1u || row == 3u || row == 4u) {
    tex_in.xyz = raw_nbt[row == 1u ? 0u : row - 2u];
  } else if (row >= 5u && row <= 12u) {
    uint t = row - 5u;
    tex_in.xy = (t % 2u) == 0u ? tex_coords[t / 2u].xy : tex_coords[t / 2u].zw;
  }

  if (((ctrl >> 2u) & 1u) == 0u)
    tex_in.z = 1.0f;
  tex_in.w = 1.0f;

  for (int k = 0; k < 3; k++)
    if (isnan(tex_in[k]))
      tex_in[k] = 1.0f;

  return tex_in;
}

vec3[8] calc_tex_gen(vec3 eye, vec3 pos, mat3x3 raw_nbt, mat3x3 nbt, uint[2] colors, uvec2 tex_mat_idx, uint aFlags, vec4[4] tex_coords){
  vec3 tex_out[8];
  for (int i = 0; i < 8; i++)
    tex_out[i] = vec3(0.0f);

  uint num_of_tex_gens = min(xf_regs.num_tex_gens, 8u);

  for(uint i = 0u; i<num_of_tex_gens;i++){
    uint ctrl = xf_regs.tex_mtx_info[i];
    uint type = (ctrl >> 4u)  &0x7u;
    bool is_stq  = ((ctrl >> 1u)  &0x1u) == 1u;

    vec3 out_tex = vec3(0.0f);
    switch(type){
      case 0u: {
        uint mat_idx = texMatIdx(tex_mat_idx, i);

        if(((aFlags >> (16u + i)) & 1u)==0u){
          if (i <= 3u)
              mat_idx = (xf_regs.matrix_index_a >> (6u + i * 6u)) & 0x3Fu;
          if (i > 3u)
              mat_idx = (xf_regs.matrix_index_b >> ((i - 4u) * 6u)) & 0x3Fu;
        }

        vec4 texCoord = get_texcoord_input(pos, raw_nbt, tex_coords, ctrl);

        out_tex.x = dot(xf_pos[mat_idx], texCoord);
        out_tex.y = dot(xf_pos[mat_idx + 1u], texCoord);
        out_tex.z = is_stq ? dot(xf_pos[mat_idx + 2u], texCoord) : 1.0f;
        break;
      }
      case 1u: {
        uint light_idx = (ctrl >> 15u) & 0x7u;
        uint src = (ctrl >> 12u) & 0x7u;

        vec3 L = xf_lights[light_idx].pos - eye;
        if (length(L) > 0.0f)
          L = normalize(L);

        out_tex.x = tex_out[src].x + dot(L, nbt[1]);
        out_tex.y = tex_out[src].y + dot(L, nbt[2]);
        out_tex.z = 1.0f;
        break;
      }
      case 2u:
        out_tex = vec3(float(colors[0] & 0xFFu) / 255.0f, float((colors[0] >> 8u) & 0xFFu) / 255.0f, 1.0f);
        break;
      case 3u:
        out_tex = vec3(float(colors[1] & 0xFFu) / 255.0f, float((colors[1] >> 8u) & 0xFFu) / 255.0f, 1.0f);
        break;
    }

    if (type == 0u && (xf_regs.dual_tex & 1u) == 1u) {
      uint post = xf_regs.post_mtx_info[i];
      if (((post >> 8u) & 1u) == 1u && length(out_tex) > 0.0f)
        out_tex = normalize(out_tex);

      vec4 post_in = vec4(out_tex, 1.0f);
      uint post_idx = post & 0x3Fu;
      out_tex = vec3(dot(xf_tex[post_idx], post_in), dot(xf_tex[post_idx + 1u], post_in), dot(xf_tex[post_idx + 2u], post_in));
    }

    if (out_tex.z == 0.0f)
      out_tex.xy = clamp(out_tex.xy / 2.0f, -1.0f, 1.0f);

    tex_out[i] = out_tex;
  }

  return tex_out;
}
