#version 410



uniform mat4 proj;

layout(std140) uniform Matrices
{
    vec4 matrix_lines[64];
};

layout (location = 0) in vec3 aPos;
layout (location = 1) in uint data; 
layout (location = 2) in vec4 aColor;
layout (location = 3) in mat3x4 mat;

out vec4 vColor;


void main()
{

    vec4 p = vec4(aPos, 1.0);
    uint row = data >> 16;

    vec3 world = vec3(dot(mat[0], p),
                      dot(mat[1], p),
                      dot(mat[2], p));

    gl_Position = proj * vec4(world, 1.0);

    vColor = aColor;
}
