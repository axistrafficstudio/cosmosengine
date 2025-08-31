#version 450 core
layout (location = 0) in vec3 inPos;
layout (location = 1) in float inRadius;
layout (location = 2) in vec4 inColor;

out VS_OUT {
    vec4 color;
} vs_out;

uniform mat4 uView;
uniform mat4 uProj;
// simple size attenuation by distance to avoid giant points near camera
float attenuate(float base, float dist){
    float s = base / (1.0 + 0.001 * dist);
    return clamp(s, 1.0, 8.0);
}

void main() {
    vec4 worldPos = vec4(inPos, 1.0);
    vec4 viewPos = uView * worldPos;
    gl_Position = uProj * viewPos;
    float dist = length(viewPos.xyz);
    gl_PointSize = attenuate(inRadius * 2.0, dist);
    vs_out.color = inColor;
}
