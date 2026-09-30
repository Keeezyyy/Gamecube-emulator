#version 410



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
layout(location = 10) in vec4  aPosMat0;
layout(location = 11) in vec4  aPosMat1;
layout(location = 12) in vec4  aPosMat2;
layout(location = 13) in uint  aFlags;
layout(location = 14) in uvec2 aTexMatIdx;

out vec4 vColor;


void main()
{

    vec4 p = vec4(aPos, 1.0);
    vec3 view = vec3(dot(aPosMat0, p), dot(aPosMat1, p), dot(aPosMat2, p));

    gl_Position = proj * vec4(view, 1.0);
}
