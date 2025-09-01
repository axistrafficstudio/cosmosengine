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
// Background & time
uniform float timeSec = 0.0;
uniform float starDensity = 0.6;
uniform float haloIntensity = 0.8;
uniform float tailAngle = 0.6;

// Hash-based noise
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453); }
float noise(vec2 p){ vec2 i=floor(p), f=fract(p); 
    float a=hash(i), b=hash(i+vec2(1,0)), c=hash(i+vec2(0,1)), d=hash(i+vec2(1,1));
    vec2 u=f*f*(3.0-2.0*f); return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

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
            warp = clamp(warp, 0.0, 0.03);
            uv = uv - normalize(d) * warp * (0.02 + 0.98 * r);
        }

        // Accretion photon ring around lensRadius with Gaussian falloff
        float ringR = lensRadius;
        float dr = abs(length(vUV - lensCenter) - ringR);
    float sigma = max(0.0035, ringWidth * lensRadius * 0.5);
        float g = exp(- (dr*dr) / (2.0 * sigma * sigma));
        // color gradient from inner (hot) to outer (cool)
        float t = clamp((length(vUV - lensCenter)) / ringR, 0.0, 1.0);
        vec3 ringCol = mix(diskInnerColor, diskOuterColor, t);
        // relativistic beaming approximation (brighter at top)
        float beaming = 1.0 + beamingStrength * clamp((vUV.y - lensCenter.y) / lensRadius, 0.0, 1.0);
        ringAdd = ringIntensity * beaming * g * ringCol;

    // Procedural accretion disk (simplified): inclined elliptical band with streaking and flicker
    // Build local coordinates aligned to diskPA with tilt squeezing minor axis
    float ca = cos(diskPA), sa = sin(diskPA);
    vec2 rel = vUV - lensCenter;
    vec2 rot = vec2(ca*rel.x + sa*rel.y, -sa*rel.x + ca*rel.y);
    float tilt = clamp(diskTilt, 0.0, 0.95);
    vec2 ell = vec2(rot.x, rot.y*(1.0-tilt));
    float er = length(ell);
    float inner = diskInnerR*lensRadius;
    float outer = diskOuterR*lensRadius;
    float feather = 0.03 * lensRadius;
    float insideInner = smoothstep(inner, inner+feather, er);
    float outsideOuter = smoothstep(outer, outer+feather, er);
    float inBand = insideInner * (1.0 - outsideOuter);
    // azimuthal streaks (along orbit) with flicker
    float phi = atan(ell.y, ell.x);
    float stripes = sin(phi*50.0 + timeSec*diskRotSpeed*4.0);
    float stripeMask = 0.45 + 0.55*pow(max(stripes, 0.0), 1.6);
    float flicker = 0.85 + 0.35*noise(vUV*vec2(300.0,120.0) + timeSec*1.3);
    vec3 diskCol = mix(diskOuterColor, diskInnerColor, clamp((diskOuterR*lensRadius - er)/( (diskOuterR-diskInnerR)*lensRadius + 1e-4), 0.0, 1.0));
    vec3 diskAdd = diskBrightness * inBand * stripeMask * flicker * diskCol;

    // Matter tail (asymmetric glow) pointing tailAngle
    vec2 dir = vec2(cos(tailAngle), sin(tailAngle));
    float along = dot(rel, dir);
    float perp = dot(rel, vec2(-dir.y, dir.x));
    float tailMask = smoothstep(0.0, lensRadius*0.6, along) * exp(-abs(perp)/(lensRadius*0.25));
    vec3 tailCol = mix(diskOuterColor, vec3(0.9,0.3,0.1), 0.6);
    vec3 tailAdd = tailMask * 0.9 * tailCol;

        // Event horizon darkening
        float horizonR = lensRadius * 0.55;
        float rcenter = length(vUV - lensCenter);
        float dark = smoothstep(horizonR, horizonR * 1.05, rcenter);
        // dark = 0 inside horizon -> fully black, 1 outside
        // We will apply this after sampling the scene by mixing to black
    vec3 scenePre = texture(sceneTex, uv).rgb;
    vec3 hdrColorBH = mix(vec3(0.0), scenePre, dark) + diskAdd + tailAdd;
        vec3 bloom = texture(bloomTex, vUV).rgb;
    // Radial halo around the lens
    float halo = haloIntensity * exp(-2.2 * max(0.0, rcenter - horizonR));
    vec3 haloCol = mix(diskOuterColor, vec3(1.0), 0.3);
    vec3 hdrColor = hdrColorBH + bloom + ringAdd + halo*haloCol;
        vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
        FragColor = vec4(mapped, 1.0);
        return;
    }
    // Starfield background with subtle twinkle
    vec3 base = texture(sceneTex, uv).rgb + texture(bloomTex, vUV).rgb;
    float sd = starDensity;
    float g = hash(floor(vUV*vec2(1400.0,800.0)));
    float star = step(1.0 - sd*0.015, g) * pow(hash(vUV*vec2(197.3,381.1)), 50.0);
    float twinkle = 0.85 + 0.15*sin(timeSec*6.0 + g*50.0);
    vec3 hdrColor = base + vec3(star*twinkle);
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    FragColor = vec4(mapped, 1.0);
}
