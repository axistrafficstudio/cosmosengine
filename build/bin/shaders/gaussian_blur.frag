#version 450 core
out vec4 FragColor;
in vec2 vUV;
uniform sampler2D inputTex;
uniform int horizontal;

const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main(){
    vec2 tex_offset = 1.0 / textureSize(inputTex, 0);
    vec3 result = texture(inputTex, vUV).rgb * weights[0];
    if (horizontal == 1) {
        for (int i = 1; i < 5; ++i) {
            result += texture(inputTex, vUV + vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
            result += texture(inputTex, vUV - vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            result += texture(inputTex, vUV + vec2(0.0, tex_offset.y * i)).rgb * weights[i];
            result += texture(inputTex, vUV - vec2(0.0, tex_offset.y * i)).rgb * weights[i];
        }
    }
    FragColor = vec4(result, 1.0);
}
