#version 450 core
layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outBright;

in VS_OUT {
    vec4 color;
    float stretch;
    vec2 dir2;
} fs_in;

uniform float bloomThreshold = 0.6;

void main() {
    // circular point
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    // anisotropic stretch along dir2
    vec2 t = normalize(fs_in.dir2);
    vec2 o = vec2(-t.y, t.x);
    float sx = 1.0 + fs_in.stretch;
    // transform into (t,o) basis, scale, and back
    float pt = dot(p, t);
    float po = dot(p, o);
    pt /= sx;
    p = t*pt + o*po;
    float r2 = dot(p,p);
    if (r2 > 1.0) discard;
    float alpha = exp(-r2 * 4.0);
    vec3 col = fs_in.color.rgb * alpha;
    outColor = vec4(col, 1.0);

    float brightness = max(max(outColor.r, outColor.g), outColor.b);
    outBright = brightness > bloomThreshold ? outColor : vec4(0.0);
}
