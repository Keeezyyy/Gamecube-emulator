#version 410

#include "types.glsl"


vec3 calc_tex_gen(vec3 eye, mat3x3 nbt, uint[2] colors){
  vec3 out_tex = vec3(0.0f);


  uint num_of_tex_gens = xf_regs.num_tex_gens;

  for(int i = 0; i<num_of_tex_gens;i++){
    uint ctrl = xf_regs.tex_mtx_info[i];
    uint type = (ctrl >> 4u)  &0x7u;
    bool is_stq  = ((ctrl >> 1u)  &0x1u) == 1;



  }

  return out_tex;
}

