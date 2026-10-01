#version 410

in vec4 vColor;

in vec3[8] tex;
in mat3x3 nbt;
in vec4 light0;
in vec4 light1;

layout(location = 0) out vec4 color;


void main() {
    //color = vColor;
    color = vec4(1.0, 0.0, 0.0, 1.0);
}
