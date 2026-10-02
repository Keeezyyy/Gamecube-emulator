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

        out_color |= ((source >> (8u * source_channel)) & 0xFFu) <<(((i*2)+j) * 8);
    }

  }
  return out_color;
}

int apply_wrap(uint wrap, uint tex_size, int coord){
  switch(wrap){
    case 0:
    case 3:
      if(coord < 0)
        return 0;
      if(coord > int(tex_size) -1)
        return int(tex_size -1);
      return coord;
    case 1:
      return int(coord & int((tex_size-1)));
    case 2:
      if((coord & int(tex_size))!= 0)
        coord = ~coord;
      return int(coord & int((tex_size -1)));
  }
  return 0;

}

uint get_index_texture_index(int s, int t, uint tex_unit_idx, TevTextureUnit tex_unit, uint tex_format){
  uint bw, bh;
  switch(tex_format){
  case 8: 
    bw=8;
    bh=8;
    break;
  case 9: 
    bw=4;
    bh=8;
    break;
  case 10: 
    bw=4;
    bh=4;
    break;
  }

  uint width_block = ((tex_unit.img0 & 0x3FFu )  + bw -1) / bw;
  uint blk = (t/bh) * width_block +(s/bw);
  uint off = (t%bh) * bw  + (s%bw);


  if(tex_format == 8){
      uint adr = (blk * 32 + off / 2);
      uint off_in_u32 = adr % 4;

      uint y = adr / 1024;
      uint x = adr % 1024;


      uint byte = texelFetch(textures_buffers, ivec3(x, y, tex_unit_idx), 0)[off_in_u32] & 0xFFu;
      return ((off & 1u) == 1) ? (uint(byte & 0xFu)) : uint(byte >> 4u);
  }else if (tex_format == 9){
      uint adr = (blk * 32 + off / 2);
      uint off_in_u32 = adr % 4;

      uint y = adr / 1024;
      uint x = adr % 1024;


      uint byte = texelFetch(textures_buffers, ivec3(x, y, tex_unit_idx), 0)[off_in_u32] & 0xFFu;
      return byte;
  }else{
      uint adr = (blk * 32 + off / 2);
      uint off_in_u32 = adr % 4;

      uint y = adr / 1024;
      uint x = adr % 1024;


      uint byte = ((texelFetch(textures_buffers, ivec3(x, y, tex_unit_idx), 0)[off_in_u32] & 0xFFu) << 8) | (texelFetch(textures_buffers, ivec3(x, y, tex_unit_idx), 0)[off_in_u32] & 0xFFu) & 0xFFu;
      return byte & 0x3FFFu;

  }

  return 0u;
}
uint expand(uint v, uint bits)
{
    v &= (1u << bits) - 1u;
    uint r = v << (8u - bits);
    r |= r >> bits;
    r |= r >> (2u * bits);   
    return r & 0xFFu;
}

uint pack_rgba(uint r, uint g, uint b, uint a)
{
    return (r & 0xFFu) | ((g & 0xFFu) << 8) | ((b & 0xFFu) << 16) | ((a & 0xFFu) << 24);
}

uint decode_tlut_color(uint u16_color, uint format)
{
    uint color = u16_color & 0xFFFFu;

    switch (format) {
    case 0u: { // IA8
        uint i = color & 0xFFu;
        return pack_rgba(i, i, i, color >> 8);
    }
    case 1u: { // RGB565
        return pack_rgba(expand(color >> 11, 5u),
                         expand(color >> 5,  6u),
                         expand(color,       5u),
                         0xFFu);
    }
    case 2u: { // RGB5A3
        if ((color >> 15) != 0u) {
            return pack_rgba(expand(color >> 10, 5u),
                             expand(color >> 5,  5u),
                             expand(color,       5u),
                             0xFFu);
        } else {
            return pack_rgba(expand(color >> 8,  4u),
                             expand(color >> 4,  4u),
                             expand(color,       4u),
                             expand(color >> 12, 3u));
        }
    }
    default:
        return pack_rgba(0xFFu, 0x00u, 0xFFu, 0xFFu);
    }
}

uint get_texel_from_texture(TevTextureUnit t_unit, uint tex_unit_idx, int s, int t, uint width, uint height){
  uint out_color = 0;
  if(((t_unit.img0 >>20) & 0xFu)<=6){

      out_color |= texelFetch(textures_buffers, ivec3(s, t, tex_unit_idx), 0).r & 0xFFu;
      out_color |= (texelFetch(textures_buffers, ivec3(s, t, tex_unit_idx), 0).g & 0xFFu) << 8;
      out_color |= (texelFetch(textures_buffers, ivec3(s, t, tex_unit_idx), 0).b & 0xFFu) << 16;
      out_color |= (texelFetch(textures_buffers, ivec3(s, t, tex_unit_idx), 0).a & 0xFFu) << 24;
  }else{
    uint format  =((t_unit.img0 >>20) & 0xFu);
    uint idx = get_index_texture_index(s, t, tex_unit_idx, t_unit, format);

    uint tmem_adr = ((t_unit.lut & 0x3FFu) << 9u);

    uint color = texelFetch(tev_tmem, int((tmem_adr + idx * 2u) / 4u)).r >> (16u * (idx & 1u));

    uint endian_swap_color = 0;
    endian_swap_color |= (color >> 8) & 0xFFu;
    endian_swap_color |= (color & 0xFFu) <<8;

    return decode_tlut_color(endian_swap_color, (t_unit.lut >> 10) & 0x3u);

  }

  return out_color;

}

uint sample_texture(TevTextureUnit t, uint tex_unit_idx, uint tex_cord_idx){
 uint out_color = 0;

 if(true){
   vec3 tex_cord = tex[tex_cord_idx];
   float q = tex_cord.z == 0.0 ? 1.0 : tex_cord.z;
   float scale_s = float((bp_regs[0x30u + 2u * tex_cord_idx] & 0xFFFFu) + 1u);
   float scale_t = float((bp_regs[0x31u + 2u * tex_cord_idx] & 0xFFFFu) + 1u);

   int s_coord = int(tex_cord.x * scale_s / q * 128.0) >> 7;
   int t_coord = int(tex_cord.y * scale_t / q * 128.0) >> 7;

   uint wrap_s = t.mode0 & 0x3u;
   uint wrap_t = (t.mode0>>2) & 0x3u;

   uint width = (t.img0 & 0x3FFu) + 1u;
   uint height = ((t.img0>>10) & 0x3FFu) + 1u;


   if(false){
     
   }else{
     s_coord = apply_wrap(wrap_s, width, s_coord);
     t_coord = apply_wrap(wrap_t, height, t_coord);

     return get_texel_from_texture(t, tex_unit_idx, s_coord, t_coord, width, height);

   }
 }

 return uint(out_color);

}

int[8] konst_fractions = int[8](255, 223, 191, 159, 128, 96, 64, 32);

int[4] _get_color_constant(uint kc, uint ka){
  int[4] out_color;
  if(kc < 8){
    out_color[0] = out_color[1] = out_color[2] = konst_fractions[kc];
  }else if(kc <12){
    out_color[0] = out_color[1] = out_color[2] = 0;
  }else if(kc <16){
    out_color[0] = tev_konst[((kc-12) * 4)];
    out_color[1] = tev_konst[((kc-12) * 4)+1];
    out_color[2] = tev_konst[((kc-12) * 4)+2];
  }else{
    out_color[0] = out_color[1] = out_color[2] =tev_konst[((kc & 3u)*4) + ((kc - 16) >> 2u)];
    }
  
  if (ka < 8)
      out_color[3] = konst_fractions[ka];
  else if (ka < 16)
      out_color[3] = 0;
  else
      out_color[3]  =tev_konst[((kc & 3u)*4) + ((kc - 16) >> 2u)];


  return out_color;
}


uint[4] rgb_num_shift = uint[4](12u, 8u, 4u, 0u);
uint[4] a_num_shift = uint[4](13u, 10u, 7u, 4u);

int color_channel(uint color, uint channel){
  return int((color >> (8u * channel)) & 0xFFu);
}

int[16] _get_color_inputs(uint ctrl, uint alpha_ctrl, int[4] prev, int[16] tev_regs, uint tex_color, uint rast_color, int[4] konst){
  int[16] inputs;

  for(uint i = 0u; i < 4u; i++){
    uint color_num = (ctrl >> rgb_num_shift[i]) & 0xFu;
    uint alpha_num = (alpha_ctrl >> a_num_shift[i]) & 0x7u;
    uint base = i * 4u;

    switch(color_num){
    case 0u:
      inputs[base + 0u] = prev[0];
      inputs[base + 1u] = prev[1];
      inputs[base + 2u] = prev[2];
      break;
    case 1u:
      inputs[base + 0u] = prev[3];
      inputs[base + 1u] = prev[3];
      inputs[base + 2u] = prev[3];
      break;
    case 2u:
    case 4u:
    case 6u:
      inputs[base + 0u] = tev_regs[(color_num / 2u) * 4u + 0u];
      inputs[base + 1u] = tev_regs[(color_num / 2u) * 4u + 1u];
      inputs[base + 2u] = tev_regs[(color_num / 2u) * 4u + 2u];
      break;
    case 3u:
    case 5u:
    case 7u:
      inputs[base + 0u] = tev_regs[((color_num - 1u) / 2u) * 4u + 3u];
      inputs[base + 1u] = tev_regs[((color_num - 1u) / 2u) * 4u + 3u];
      inputs[base + 2u] = tev_regs[((color_num - 1u) / 2u) * 4u + 3u];
      break;
    case 8u:
      inputs[base + 0u] = color_channel(tex_color, 0u);
      inputs[base + 1u] = color_channel(tex_color, 1u);
      inputs[base + 2u] = color_channel(tex_color, 2u);
      break;
    case 9u:
      inputs[base + 0u] = color_channel(tex_color, 3u);
      inputs[base + 1u] = color_channel(tex_color, 3u);
      inputs[base + 2u] = color_channel(tex_color, 3u);
      break;
    case 10u:
      inputs[base + 0u] = color_channel(rast_color, 0u);
      inputs[base + 1u] = color_channel(rast_color, 1u);
      inputs[base + 2u] = color_channel(rast_color, 2u);
      break;
    case 11u:
      inputs[base + 0u] = color_channel(rast_color, 3u);
      inputs[base + 1u] = color_channel(rast_color, 3u);
      inputs[base + 2u] = color_channel(rast_color, 3u);
      break;
    case 12u:
      inputs[base + 0u] = 255;
      inputs[base + 1u] = 255;
      inputs[base + 2u] = 255;
      break;
    case 13u:
      inputs[base + 0u] = 128;
      inputs[base + 1u] = 128;
      inputs[base + 2u] = 128;
      break;
    case 14u:
      inputs[base + 0u] = konst[0];
      inputs[base + 1u] = konst[1];
      inputs[base + 2u] = konst[2];
      break;
    case 15u:
      inputs[base + 0u] = 0;
      inputs[base + 1u] = 0;
      inputs[base + 2u] = 0;
      break;
    }

    switch(alpha_num){
    case 0u:
      inputs[base + 3u] = prev[3];
      break;
    case 1u:
    case 2u:
    case 3u:
      inputs[base + 3u] = tev_regs[alpha_num * 4u + 3u];
      break;
    case 4u:
      inputs[base + 3u] = color_channel(tex_color, 3u);
      break;
    case 5u:
      inputs[base + 3u] = color_channel(rast_color, 3u);
      break;
    case 6u:
      inputs[base + 3u] = konst[3];
      break;
    case 7u:
      inputs[base + 3u] = 0;
      break;
    }
  }

  return inputs;
}


void main() {
  uint gen_mode = bp_regs[0];
  uint num_of_steps = ((gen_mode >> 10u) & 0xFu) + 1u;

  int[16] tev_regs = tev_reg_start;


  int[4] prev = int[4](tev_reg_start[0], tev_reg_start[1],tev_reg_start[2],tev_reg_start[3]);

  int[4] c;
  uint tex_color;

  for(uint i = 0; i<num_of_steps;i++){
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

    if (((tev_order >> (is_even ? 6 : 18)) & 1u) == 1 ){
      if(((gen_mode & 0xFu) != 0)){
        TevTextureUnit t = tex_units[tex_unit];

        tex_color = sample_texture(t, tex_unit, tex_cords);
        tex_color = swap_color(swap_index_texture_color, tex_color);
      }
    }

    uint color_constant = ((bp_regs[0xf6 + (i/2)]) >> (i%2 == 0 ? 4 : 14)) & 0x1Fu;
    uint alpha_constant = ((bp_regs[0xf6 + (i/2)]) >> (i%2 == 0 ? 9 : 19)) & 0x1Fu;

    
    int[4] konst = _get_color_constant(color_constant, alpha_constant);


    int[16] input_color = _get_color_inputs(bp_regs[0xc0 + (2*i)], bp_regs[0xc1 + (2*i)], prev, tev_regs,tex_color, rast_color, konst );



  }

    
  
  vec4 final_color = pack_light(tex_color);
  color = vec4(final_color.rgb, 1.0);
}
