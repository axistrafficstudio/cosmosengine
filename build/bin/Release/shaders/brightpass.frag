#version 450 core
out vec4 FragColor;
in vec2 vUV;
uniform sampler2D inputTex;
const float bloomThreshold = 1.2;
void main(){
    vec3 col = texture(inputTex, vUV).rgb;
    float brightness = max(max(col.r, col.g), col.b);
    if (brightness > bloomThreshold)
        FragColor = vec4(col, 1.0);
    else
        FragColor = vec4(0.0);
}
