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
  return convert_normilized_vec4_to_rgba(light0);

  break;
case 1:
  return convert_normilized_vec4_to_rgba(light1);
  break;
default:
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

  uint width_block = ((tex_unit.img0 & 0x3FFu ) + 1u + bw -1) / bw;
  uint blk = (t/bh) * width_block +(s/bw);
  uint off = (t%bh) * bw  + (s%bw);


  if(tex_format == 8){
      uint adr = (blk * 32 + off / 2);
      uint off_in_u32 = adr % 4;

      uint y = adr / 4096;
      uint x = (adr / 4) % 1024;


      uint byte = texelFetch(textures_buffers, ivec3(x, y, tex_unit_idx), 0)[off_in_u32] & 0xFFu;
      return ((off & 1u) == 1) ? (uint(byte & 0xFu)) : uint(byte >> 4u);
  }else if (tex_format == 9){
      uint adr = (blk * 32 + off);
      uint off_in_u32 = adr % 4;

      uint y = adr / 4096;
      uint x = (adr / 4) % 1024;


      uint byte = texelFetch(textures_buffers, ivec3(x, y, tex_unit_idx), 0)[off_in_u32] & 0xFFu;
      return byte;
  }else{
      uint adr = (blk * 32 + off * 2);
      uint off_in_u32 = adr % 4;

      uint y = adr / 4096;
      uint x = (adr / 4) % 1024;


      uint byte = ((texelFetch(textures_buffers, ivec3(x, y, tex_unit_idx), 0)[off_in_u32] & 0xFFu) << 8) | (texelFetch(textures_buffers, ivec3(x, y, tex_unit_idx), 0)[off_in_u32 + 1] & 0xFFu);
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
      out_color[3]  =tev_konst[((ka & 3u)*4) + ((ka - 16) >> 2u)];


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


struct CombineConfig {
  uint bias;
  uint operation;
  uint clamp;
  uint scale;
  uint dest;
};

int[4] shrink_rgbas16(int[4] v){
  for(int i = 0; i < 4; i++)
    v[i] = v[i] & 0xFF;
  return v;
}

int[4] expand_rgba8(int[4] v){
  for(int i = 0; i < 4; i++)
    v[i] = v[i] + (v[i] / 128);
  return v;
}

int _clamp(int t, CombineConfig conf){
  if(conf.clamp != 0u)
    t = t < 0 ? 0 : t > 255 ? 255 : t;
  else
    t = t < -1024 ? -1024 : t > 1023 ? 1023 : t;

  return t;
}

uint _cmp_value(int[4] v, uint width){
  uint x = uint(v[0] & 0xFF);
  if(width >= 1u)
    x |= uint(v[1] & 0xFF) << 8;
  if(width >= 2u)
    x |= uint(v[2] & 0xFF) << 16;
  return x;
}

bool _compare(uint a, uint b, CombineConfig conf){
  return conf.operation == 0u ? a > b : a == b;
}

int _compare_channel(int a, int b, int c, int d, bool all, CombineConfig conf){
  bool cond = conf.scale == 3u ? _compare(uint(a & 0xFF), uint(b & 0xFF), conf) : all;
  return _clamp(d + (cond ? c : 0), conf);
}

int[3] mix_bias = int[3](0, 128, -128);

int _mix(int a, int b, int c, int d, CombineConfig conf){
  int t = a * (256 - c) + b * c;

  if(conf.scale == 1u)
    t *= 2;
  else if(conf.scale == 2u)
    t *= 4;

  if(conf.scale != 3u)
    t += conf.operation == 1u ? 127 : 128;

  t >>= 8;

  if(conf.operation == 1u)
    t = -t;

  int dd = d + mix_bias[conf.bias];
  if(conf.scale == 1u)
    dd *= 2;
  else if(conf.scale == 2u)
    dd *= 4;
  t += dd;

  if(conf.scale == 3u)
    t >>= 1;

  return _clamp(t, conf);
}

void _write_dest(uint dest, uint ch, int v, inout int[4] prev, inout int[16] tev_regs){
  if(dest == 0u)
    prev[ch] = v;
  else
    tev_regs[dest * 4u + ch] = v;
}

int[4] tev_calc_core(CombineConfig color_conf, CombineConfig alpha_conf, int[16] inputs, inout int[4] prev, inout int[16] tev_regs){
  int[4] out_color = int[4](0, 0, 0, 0);

  int[4] ia = shrink_rgbas16(int[4](inputs[0], inputs[1], inputs[2], inputs[3]));
  int[4] ib = shrink_rgbas16(int[4](inputs[4], inputs[5], inputs[6], inputs[7]));
  int[4] ic = shrink_rgbas16(int[4](inputs[8], inputs[9], inputs[10], inputs[11]));
  int[4] id = int[4](inputs[12], inputs[13], inputs[14], inputs[15]);

  int[4] c = ic;
  ic = expand_rgba8(ic);

  if(color_conf.bias == 3u){
    bool all = _compare(_cmp_value(ia, color_conf.scale), _cmp_value(ib, color_conf.scale), color_conf);
    out_color[0] = _compare_channel(ia[0], ib[0], c[0], id[0], all, color_conf);
    out_color[1] = _compare_channel(ia[1], ib[1], c[1], id[1], all, color_conf);
    out_color[2] = _compare_channel(ia[2], ib[2], c[2], id[2], all, color_conf);
  }else{
    out_color[0] = _mix(ia[0], ib[0], ic[0], id[0], color_conf);
    out_color[1] = _mix(ia[1], ib[1], ic[1], id[1], color_conf);
    out_color[2] = _mix(ia[2], ib[2], ic[2], id[2], color_conf);
  }

  if(alpha_conf.bias == 3u){
    bool all = _compare(_cmp_value(ia, alpha_conf.scale), _cmp_value(ib, alpha_conf.scale), alpha_conf);
    out_color[3] = _compare_channel(ia[3], ib[3], c[3], id[3], all, alpha_conf);
  }else{
    out_color[3] = _mix(ia[3], ib[3], ic[3], id[3], alpha_conf);
  }

  _write_dest(color_conf.dest, 0u, out_color[0], prev, tev_regs);
  _write_dest(color_conf.dest, 1u, out_color[1], prev, tev_regs);
  _write_dest(color_conf.dest, 2u, out_color[2], prev, tev_regs);
  _write_dest(alpha_conf.dest, 3u, out_color[3], prev, tev_regs);

  return out_color;
}

uint _z_texture(uint raw_tex, uint z){
  uint _bias = bp_regs[0xF4] & 0xFFFFFFu;
  uint fmt = bp_regs[0xF5] & 0x3u;
  uint operation = (bp_regs[0xF5] >> 2) & 0x3u;

  if(operation ==0)
    return z;


  uint t;
  switch(fmt){
    case 0:
      t = raw_tex>>24;
      break;
    case 1:
      t = ((raw_tex>>24)<<8) | ((raw_tex)& 0xFFu);
        break;
    default:
      t =   ((((raw_tex)& 0xFFu))<<16) | ((((raw_tex>>8)& 0xFFu))<<8) |  ((((raw_tex>>16)& 0xFFu)));
        break;
  }

  t+= _bias;

  return ((operation == 1) ? z + t : t) & 0xFFFFFFu;
}

 bool _alpha_cmp(uint op, int a, int ref) {
    switch (op) {
      case 0u: return false;      
      case 1u: return a <  ref;   
      case 2u: return a == ref;   
      case 3u: return a <= ref;   
      case 4u: return a >  ref;   
      case 5u: return a != ref;   
      case 6u: return a >= ref;   
      default: return true;      
    }
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

    uint reg = bp_regs[0xc0 + (2*i)];

    uint _bias = (reg>>16) &0x3u;
    uint operation = (reg>>18) &0x1u;
    uint _clamp = (reg>>19) &0x1u;
    uint scale = (reg>>20) &0x3u;
    uint dest = (reg>>22) &0x3u;


    uint alpha_reg = bp_regs[0xc1 + (2*i)];

    uint alpha_bias = (alpha_reg>>16) &0x3u;
    uint alpha_operation = (alpha_reg>>18) &0x1u;
    uint alpha_clamp = (alpha_reg>>19) &0x1u;
    uint alpha_scale = (alpha_reg>>20) &0x3u;
    uint alpha_dest = (alpha_reg>>22) &0x3u;

    CombineConfig conf = CombineConfig(_bias, operation, _clamp, scale, dest);
    CombineConfig alpha_conf = CombineConfig(alpha_bias, alpha_operation, alpha_clamp, alpha_scale, alpha_dest);

    c = tev_calc_core(conf, alpha_conf, input_color, prev, tev_regs);
  }



  uint new_z = _z_texture(tex_color, uint(gl_FragCoord.z * 16777215));
  gl_FragDepth = float(new_z) / 16777215.0;



  c[3] = clamp(c[3], 0, 255);

    if (((bp_regs[0x42] >> 8) & 1u)==1) {
        c[3] = int(uint(bp_regs[0x42] & 0xFFu));
    }

  vec4 final_color = vec4(clamp(c[0], 0, 255), clamp(c[1], 0, 255), clamp(c[2], 0, 255), clamp(c[3], 0, 255)) / 255.0;

  uint ac = bp_regs[0xF3];
  bool r0 = _alpha_cmp((ac >> 16) & 7u, c[3], int(ac & 0xFFu));
  bool r1 = _alpha_cmp((ac >> 19) & 7u, c[3], int((ac >> 8) & 0xFFu));
  uint logic = (ac >> 22) & 3u;   
  bool pass = logic == 0u ? (r0 && r1)
            : logic == 1u ? (r0 || r1)
            : logic == 2u ? (r0 != r1)
            :               (r0 == r1);
  if (!pass)
    discard;


  color = final_color;
}
