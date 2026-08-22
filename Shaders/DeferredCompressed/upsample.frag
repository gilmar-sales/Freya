#version 450

// 9-tap tent upsample (1-2-1 / 2-4-2 / 1-2-1).

layout(binding = 0) uniform sampler2D inDownsample;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = inTexCoord;
    vec2 o = 2.0 / vec2(textureSize(inDownsample, 0));

    vec3 s = texture(inDownsample, uv + vec2(-o.x, -o.y)).rgb;
    s += texture(inDownsample, uv + vec2(0.0, -o.y)).rgb * 2.0;
    s += texture(inDownsample, uv + vec2(o.x, -o.y)).rgb;

    s += texture(inDownsample, uv + vec2(-o.x, 0.0)).rgb * 2.0;
    s += texture(inDownsample, uv).rgb * 4.0;
    s += texture(inDownsample, uv + vec2(o.x, 0.0)).rgb * 2.0;

    s += texture(inDownsample, uv + vec2(-o.x, o.y)).rgb;
    s += texture(inDownsample, uv + vec2(0.0, o.y)).rgb * 2.0;
    s += texture(inDownsample, uv + vec2(o.x, o.y)).rgb;

    outColor = vec4(s / 16.0, 1.0);
}
