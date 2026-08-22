#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inScene;
layout(set = 0, binding = 1) uniform sampler2D inDepth;

#include "Include/post_mask.inc"

layout(push_constant) uniform UnderwaterPush {
    float time;
    float strength;
    float tintStrength;
    float fogDensity;
    vec4  tintColor;
    float reverseZ;
    float maxDepth;
    float _pad0;
    float _pad1;
} push;

float Hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float Noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = Hash(i);
    float b = Hash(i + vec2(1.0, 0.0));
    float c = Hash(i + vec2(0.0, 1.0));
    float d = Hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float LinearDepth(float d) {
    // Approximate: treat non-sky depth as 0..1 distance factor.
    if (push.reverseZ > 0.5)
        return clamp(1.0 - d, 0.0, 1.0);
    return clamp(d, 0.0, 1.0);
}

void main() {
    float depth = texture(inDepth, inUV).r;
    bool sky = push.reverseZ > 0.5 ? depth <= 1e-6 : depth >= 0.999999;
    float dist = sky ? 1.0 : LinearDepth(depth);
    dist = clamp(dist / max(push.maxDepth, 1e-3), 0.0, 1.0);

    float t = push.time;
    vec2 warp =
        vec2(Noise(inUV * 12.0 + vec2(t * 0.35, t * 0.2)),
             Noise(inUV * 14.0 + vec2(-t * 0.25, t * 0.4))) *
        2.0 - 1.0;
    float amp = push.strength * (0.35 + 0.65 * dist);
    vec2 uv = inUV + warp * amp * 0.02;

    vec3 scene = texture(inScene, uv).rgb;
    vec3 tinted = mix(scene, scene * push.tintColor.rgb, push.tintStrength);
    float fog = 1.0 - exp(-push.fogDensity * dist);
    vec3 fogged = mix(tinted, push.tintColor.rgb * 0.35, fog);

    uint matId = SampleMaterialId(inUV);
    if (!MaterialIncluded(matId) && !sky) {
        outColor = vec4(texture(inScene, inUV).rgb, 1.0);
        return;
    }
    outColor = vec4(fogged, 1.0);
}
