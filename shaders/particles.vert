#version 450 core
layout (location = 0) in vec3 inPos;
layout (location = 1) in float inRadius;
layout (location = 2) in vec4 inColor;

out VS_OUT {
    vec4 color;
} vs_out;

uniform mat4 uView;
uniform mat4 uProj;

void main() {
    vec4 worldPos = vec4(inPos, 1.0);
    gl_Position = uProj * uView * worldPos;
    gl_PointSize = clamp(inRadius * 2.0, 1.0, 10.0);
    vs_out.color = inColor;
}
