#version 410



uniform mat4 proj;

layout(std140) uniform Matrices
{
    vec4 matrix_lines[64];
};

layout (location = 0) in vec3 aPos;
layout (location = 1) in uint data; 

void main()
{
    uint row = data >> 16;
    vec4 p = vec4(aPos, 1.0);

    vec3 world = vec3(dot(matrix_lines[row + 0], p),
                      dot(matrix_lines[row + 1], p),
                      dot(matrix_lines[row + 2], p));

    gl_Position = proj * vec4(world, 1.0);
}
