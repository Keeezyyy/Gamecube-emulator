#version 410
#include "lib/types.glsl"
#include "lib/light.glsl"

uniform int[16] tev_konst;
uniform int[16] tev_reg_start;

in vec4 vColor;


in vec3[8] tex;
in float[8] lod;
in mat3x3 nbt;
in vec4 light0;
in vec4 light1;
in vec4 col0;
in vec4 col1;

layout(location = 0) out vec4 color;


uint get_rast_color(uint color_type){
  switch(color_type){
case 0:
  return convert_normilized_vec4_to_rgba(col0);

  break;
case 1:
  return convert_normilized_vec4_to_rgba(col1);
  break;
case 2:
  return uint(0);
  break;
  }
}

uint swap_color(uint swap_index, uint source){
  uint out_color = 0;
  for(int i = 0;i<2;i++){
    uint reg = bp_regs[0xf6 + 2 * swap_index + i];
    for(int j = 0;j<2;j++){
        uint source_channel = (reg >> (2u * j)) & 0x3u;

        out_color |= ((source >> (3-source_channel)) & 0xFFu) <<(((i*2)+j) * 8);
    }

  }
  return out_color;
}

void main() {
  uint gen_mode = bp_regs[0];
  uint num_of_steps = ((gen_mode >> 10u) & 0xFu) + 1u;

  int[4] prev = int[4](tev_reg_start[0], tev_reg_start[1],tev_reg_start[2],tev_reg_start[3]);

  int[4] c;
  uint tex_color;

  for(uint i = 0; i<gen_mode;i++){
    uint tev_order = bp_regs[0x28 + i / 2];

    bool is_even = i %2 ==0;


   uint swap_index_rast_color = bp_regs[0xC1 + (i * 2)] & 0x3u;
   uint rast_color = get_rast_color((tev_order >> (is_even ? 7 : 19)) & 0x7u);
   rast_color = swap_color(swap_index_rast_color, rast_color);


   uint swap_index_texture_color = (bp_regs[0xC1 + (i * 2)]>>2) & 0x3u;
   uint tex_unit = (tev_order >> (is_even ? 0 : 12)) & 0x7u;
   uint tex_cords = (tev_order >> (is_even ? 3 : 15)) & 0x7u;
   if(tex_cords >= (gen_mode &0xFu))
    tex_cords = 0;

   tex_color = 0;

        if (((tev_order >> (is_even ? 6 : 18)) & 1) && (gen_mode & 0xF)){

        }
    



  }

    
  color = vec4(1.0, 0.0, 0.0, 1.0);
}
