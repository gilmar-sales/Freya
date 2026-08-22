#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inScene;
layout(set = 0, binding = 1) uniform sampler2D inDepth;

#include "Include/post_mask.inc"

layout(push_constant) uniform HeatPush {
    float time;
    float strength;
    float speed;
    float reverseZ;
} push;

float Hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float Noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = Hash(i);
    float b = Hash(i + vec2(1.0, 0.0));
    float c = Hash(i + vec2(0.0, 1.0));
    float d = Hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main() {
    vec3 scene = texture(inScene, inUV).rgb;
    float depth = texture(inDepth, inUV).r;
    bool sky = push.reverseZ > 0.5 ? depth <= 1e-6 : depth >= 0.999999;
    uint matId = SampleMaterialId(inUV);

    // BindMaterial selects "hot" surfaces; count==0 → whole frame shimmers.
    if (sky || !MaterialIncluded(matId)) {
        outColor = vec4(scene, 1.0);
        return;
    }

    float t = push.time * max(push.speed, 0.0);
    float n1 = Noise(inUV * 40.0 + vec2(0.0, t));
    float n2 = Noise(inUV * 55.0 + vec2(t * 0.7, -t));
    vec2 offset = (vec2(n1, n2) * 2.0 - 1.0) * push.strength * 0.015;
    vec3 warped = texture(inScene, inUV + offset).rgb;
    // Slight warm bias on the haze itself.
    warped *= vec3(1.05, 1.0, 0.95);
    outColor = vec4(warped, 1.0);
}
