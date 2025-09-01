#version 450 core
layout (location = 0) in vec3 inPos;
layout (location = 1) in float inRadius;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec3 inVel;

out VS_OUT {
    vec4 color;
    float stretch;
    vec2 dir2;
} vs_out;

uniform mat4 uView;
uniform mat4 uProj;
// simple size attenuation by distance to avoid giant points near camera
float attenuate(float base, float dist){
    float s = base / (1.0 + 0.001 * dist);
    return clamp(s, 2.5, 14.0);
}

void main() {
    vec4 worldPos = vec4(inPos, 1.0);
    vec4 viewPos = uView * worldPos;
    gl_Position = uProj * viewPos;
    float dist = length(viewPos.xyz);
    float base = attenuate(inRadius * 6.0, dist);
    // simple stretch factor from velocity magnitude
    float vmag = length(inVel);
    vs_out.stretch = clamp(vmag * 0.02, 0.0, 2.5);
    // view-space velocity direction projected to screen axes (x,y)
    vec3 vView = mat3(uView) * inVel;
    vec2 d = normalize(vec2(vView.x, vView.y) + 1e-6);
    vs_out.dir2 = d;
    gl_PointSize = base * (1.0 + 0.25*vs_out.stretch);
    vs_out.color = inColor;
}
