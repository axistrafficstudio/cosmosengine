#version 450 core
out vec4 FragColor;
in vec2 vUV;
uniform sampler2D sceneTex;
uniform sampler2D bloomTex;
uniform float exposure = 1.0;
uniform int lensEnabled = 0;
uniform vec2 lensCenter = vec2(0.5, 0.5);
uniform float lensRadius = 0.15; // normalized
uniform float lensStrength = 0.25; // refraction intensity

void main() {
    vec2 uv = vUV;
    if (lensEnabled == 1) {
        vec2 d = uv - lensCenter;
        float r = length(d);
        if (r < lensRadius) {
            float k = (lensRadius - r) / lensRadius; // 1 at center -> 0 at edge
            float warp = lensStrength * k * k;
            uv = uv - normalize(d) * warp * (0.02 + 0.98 * r);
        }
    }
    vec3 hdrColor = texture(sceneTex, uv).rgb + texture(bloomTex, vUV).rgb;
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    FragColor = vec4(mapped, 1.0);
}
