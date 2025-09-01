#version 450 core
out vec4 FragColor;
in vec2 vUV;
uniform sampler2D sceneTex;
uniform sampler2D bloomTex;
uniform float exposure = 1.0;
uniform int lensEnabled = 0;
uniform vec2 lensCenter = vec2(0.5, 0.5);
// Lensing and accretion ring controls
uniform float lensStrength = 0.25; // refraction intensity
uniform float lensRadiusScale = 1.0; // scales default radius
uniform float ringIntensity = 1.0;
uniform float ringWidth = 0.05; // normalized to lens radius
uniform float beamingStrength = 0.5; // brightness boost towards top
uniform vec3 diskInnerColor = vec3(1.2, 0.6, 0.2);
uniform vec3 diskOuterColor = vec3(1.0, 0.8, 0.5);
// Disk shape/orientation
uniform float diskInnerR = 0.6;
uniform float diskOuterR = 1.6;
uniform float diskTilt = 0.6;
uniform float diskPA = 0.0;
uniform float diskBrightness = 1.0;
uniform float diskRotSpeed = 1.5;

void main() {
    vec2 uv = vUV;
    float lensRadius = max(0.05, lensRadiusScale * 0.23);
    vec3 ringAdd = vec3(0.0);
    if (lensEnabled == 1) {
        vec2 d = uv - lensCenter;
        float r = length(d);
        if (r < lensRadius) {
            float k = (lensRadius - r) / lensRadius; // 1 at center -> 0 at edge
            float warp = lensStrength * k * k;
            uv = uv - normalize(d) * warp * (0.02 + 0.98 * r);
        }

        // Accretion photon ring around lensRadius with Gaussian falloff
        float ringR = lensRadius;
        float dr = abs(length(vUV - lensCenter) - ringR);
        float sigma = max(0.0025, ringWidth * lensRadius * 0.5);
        float g = exp(- (dr*dr) / (2.0 * sigma * sigma));
        // color gradient from inner (hot) to outer (cool)
        float t = clamp((length(vUV - lensCenter)) / ringR, 0.0, 1.0);
        vec3 ringCol = mix(diskInnerColor, diskOuterColor, t);
        // relativistic beaming approximation (brighter at top)
        float beaming = 1.0 + beamingStrength * clamp((vUV.y - lensCenter.y) / lensRadius, 0.0, 1.0);
        ringAdd = ringIntensity * beaming * g * ringCol;

    // Procedural accretion disk (simplified): an inclined elliptical band with streaking
    // Build local coordinates aligned to diskPA with tilt squeezing minor axis
    float ca = cos(diskPA), sa = sin(diskPA);
    vec2 rel = vUV - lensCenter;
    vec2 rot = vec2(ca*rel.x + sa*rel.y, -sa*rel.x + ca*rel.y);
    float tilt = clamp(diskTilt, 0.0, 0.95);
    vec2 ell = vec2(rot.x, rot.y*(1.0-tilt));
    float er = length(ell);
    float inBand = smoothstep(diskOuterR*lensRadius, diskInnerR*lensRadius, er);
    // streaks moving along +x with speed
    float stripes = sin( (rot.x*40.0) + (diskRotSpeed*100.0)*fract(sin(dot(vUV, vec2(12.9898,78.233))) * 43758.5453) );
    float stripeMask = 0.4 + 0.6*pow(max(stripes,0.0), 2.0);
    vec3 diskCol = mix(diskOuterColor, diskInnerColor, clamp((diskOuterR*lensRadius - er)/( (diskOuterR-diskInnerR)*lensRadius + 1e-4), 0.0, 1.0));
    vec3 diskAdd = diskBrightness * inBand * stripeMask * diskCol;

        // Event horizon darkening
        float horizonR = lensRadius * 0.55;
        float rcenter = length(vUV - lensCenter);
        float dark = smoothstep(horizonR, horizonR * 1.05, rcenter);
        // dark = 0 inside horizon -> fully black, 1 outside
        // We will apply this after sampling the scene by mixing to black
    vec3 scenePre = texture(sceneTex, uv).rgb;
    vec3 hdrColorBH = mix(vec3(0.0), scenePre, dark) + diskAdd;
        vec3 bloom = texture(bloomTex, vUV).rgb;
        vec3 hdrColor = hdrColorBH + bloom + ringAdd;
        vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
        FragColor = vec4(mapped, 1.0);
        return;
    }
    vec3 hdrColor = texture(sceneTex, uv).rgb + texture(bloomTex, vUV).rgb;
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    FragColor = vec4(mapped, 1.0);
}
