#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in uint inPacked;

layout(location = 0) out vec4 outColor;

void main() {
    bool is3d      = (inPacked         & 0xFFu) != 0u;
    bool hasMatIdx = ((inPacked >> 8u) & 0xFFu) != 0u;
    uint matIdx    =  inPacked >> 16u;
    // ...
    gl_Position = vec4(inPos, 1.0);
    outColor    = vec4(1.0);
}
