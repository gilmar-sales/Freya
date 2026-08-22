#version 450

// Jimenez 13-tap dual-filter downsample.

layout(binding = 0) uniform sampler2D inThreshold;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = inTexCoord;
    vec2 texelSize = 1.0 / vec2(textureSize(inThreshold, 0));

    vec3 a =
        texture(inThreshold, uv + texelSize * vec2(-1.0, -1.0)).rgb;
    vec3 b = texture(inThreshold, uv + texelSize * vec2(0.0, -1.0)).rgb;
    vec3 c =
        texture(inThreshold, uv + texelSize * vec2(1.0, -1.0)).rgb;
    vec3 d =
        texture(inThreshold, uv + texelSize * vec2(-0.5, -0.5)).rgb;
    vec3 e =
        texture(inThreshold, uv + texelSize * vec2(0.5, -0.5)).rgb;
    vec3 f = texture(inThreshold, uv + texelSize * vec2(-1.0, 0.0)).rgb;
    vec3 g = texture(inThreshold, uv).rgb;
    vec3 h = texture(inThreshold, uv + texelSize * vec2(1.0, 0.0)).rgb;
    vec3 i =
        texture(inThreshold, uv + texelSize * vec2(-0.5, 0.5)).rgb;
    vec3 j =
        texture(inThreshold, uv + texelSize * vec2(0.5, 0.5)).rgb;
    vec3 k =
        texture(inThreshold, uv + texelSize * vec2(-1.0, 1.0)).rgb;
    vec3 l = texture(inThreshold, uv + texelSize * vec2(0.0, 1.0)).rgb;
    vec3 m =
        texture(inThreshold, uv + texelSize * vec2(1.0, 1.0)).rgb;

    vec3 color = (d + e + i + j) * 0.5;
    color += (a + b + f + g) * 0.125;
    color += (b + c + g + h) * 0.125;
    color += (f + g + k + l) * 0.125;
    color += (g + h + l + m) * 0.125;
    color *= 0.25;

    outColor = vec4(color, 1.0);
}
