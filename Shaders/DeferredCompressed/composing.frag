#version 450

// Final composite: opaque (+ optional HDR tonemap), translucent, bloom.

layout(binding = 0) uniform sampler2D inOpaque;
layout(binding = 1) uniform sampler2D inTranslucent;
layout(binding = 2) uniform sampler2D inBloom;

layout(push_constant) uniform PushConstants {
    float tonemapHdr;    // 1 = ACES+gamma, 0 = LDR passthrough
    float bloomStrength;
} pc;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec4 opaqueColor = texture(inOpaque, inTexCoord);
    vec4 transColor = texture(inTranslucent, inTexCoord);
    vec4 bloomColor = texture(inBloom, inTexCoord);

    vec3 color = mix(opaqueColor.rgb, transColor.rgb, transColor.a);
    color += bloomColor.rgb * pc.bloomStrength;

    if (pc.tonemapHdr > 0.5) {
        color = ACESFilm(color);
        color = pow(color, vec3(1.0 / 2.2));
    }

    outColor = vec4(color, 1.0);
}
