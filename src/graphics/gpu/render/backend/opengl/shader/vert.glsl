#version 410

#include "lib/types.glsl"
#include "lib/light.glsl"
#include "lib/texture.glsl"

uniform mat4 proj;

layout(location = 0)  in vec3  aPos;
layout(location = 1)  in vec3  aNormal;
layout(location = 2)  in vec3  aBinormal;
layout(location = 3)  in vec3  aTangent;
layout(location = 4)  in vec4  aColor0;
layout(location = 5)  in vec4  aColor1;
layout(location = 6)  in vec4  aTex01;
layout(location = 7)  in vec4  aTex23;
layout(location = 8)  in vec4  aTex45;
layout(location = 9)  in vec4  aTex67;
layout(location = 10) in uint  aFlags;
layout(location = 11) in uvec2 aTexMatIdx;
layout(location = 12) in uint aMatIndices;


out vec4 vColor;

out vec3[8] tex;
out float[8] lod;
out mat3x3 nbt;
out vec4 light0;
out vec4 light1;
out vec4 col0;
out vec4 col1;


void main()
{

    uint pos_mat_idx  = aMatIndices & 0xFFu;
    uint norm_mat_idx = min((aMatIndices >> 8u) & 31u, 29u);

    vec4 p = vec4(aPos, 1.0);
    vec3 pos = vec3(dot(xf_pos[pos_mat_idx], p), dot(xf_pos[pos_mat_idx + 1u], p), dot(xf_pos[pos_mat_idx + 2u], p));
      
    
    mat3x3 normMat = mat3x3(xf_norm[norm_mat_idx], xf_norm[norm_mat_idx + 1u], xf_norm[norm_mat_idx + 2u]);

    vec3 n = aNormal * normMat;
    vec3 b = aBinormal * normMat;
    vec3 t = aTangent * normMat;

    nbt = mat3x3(n,b,t);

    uint colors[2] = uint[2](calc_light(pos, mat3x3(n,b,t), convert_normilized_vec4_to_rgba(aColor0), 0), calc_light(pos, mat3x3(n,b,t), convert_normilized_vec4_to_rgba(aColor1), 1));
    light0 = pack_light(colors[0]);
    light1 = pack_light(colors[1]);

    tex = calc_tex_gen(pos, aPos, mat3x3(aNormal, aBinormal, aTangent), mat3x3(n,b,t), colors, aTexMatIdx, aFlags, vec4[4](aTex01, aTex23, aTex45, aTex67));


    col0 = aColor0;
    col1 = aColor1;
    
    for(uint i = 0; i<8; i++){
      lod[i] = 0.0f;
    }
    

  gl_Position = proj * vec4(pos, 1.0);
}
