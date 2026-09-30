#version 410

#include "lib/types.glsl"
#include "lib/light.glsl"

uniform mat4 proj;
uniform vec4 xf_pos[64];  
uniform vec3 xf_norm[32];


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

    uint light0 = calc_light(pos, mat3x3(n,b,t), convert_normilized_vec4_to_rgba(aColor0), 0);
    uint light1 = calc_light(pos, mat3x3(n,b,t), convert_normilized_vec4_to_rgba(aColor1), 1);
    
    

  gl_Position = proj * vec4(pos, 1.0);
}
