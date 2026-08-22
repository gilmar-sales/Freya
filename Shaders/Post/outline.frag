#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inScene;
layout(set = 0, binding = 1) uniform sampler2D inDepth;
layout(set = 0, binding = 2) uniform sampler2D inNormal;

#include "Include/post_mask.inc"

layout(push_constant) uniform OutlinePush {
    float edgeDepthScale;
    float edgeNormalScale;
    float strength;
    float reverseZ;
    vec4  edgeColor;
    float edgeWidth;
    float _pad0;
    float _pad1;
    float _pad2;
} push;

float SampleDepth(vec2 uv) { return texture(inDepth, uv).r; }

vec3 SampleNormal(vec2 uv) {
    return normalize(texture(inNormal, uv).rgb * 2.0 - 1.0);
}

bool IsSky(float depth) {
    if (push.reverseZ > 0.5)
        return depth <= 1e-6;
    return depth >= 0.999999;
}

void main() {
    vec3 scene = texture(inScene, inUV).rgb;
    float depth = SampleDepth(inUV);
    uint matId = SampleMaterialId(inUV);
    if (IsSky(depth) || !MaterialIncluded(matId)) {
        outColor = vec4(scene, 1.0);
        return;
    }

    ivec2 size = textureSize(inDepth, 0);
    vec2 texel = 1.0 / vec2(size);
    float widthPx =
        max(push.edgeWidth, 1.0) * max(float(size.y) / 1080.0, 1.0);
    vec2 step = texel * widthPx;

    float gx = SampleDepth(inUV + vec2(step.x, 0.0)) -
               SampleDepth(inUV - vec2(step.x, 0.0));
    float gy = SampleDepth(inUV + vec2(0.0, step.y)) -
               SampleDepth(inUV - vec2(0.0, step.y));
    float edgeDepth = length(vec2(gx, gy)) * push.edgeDepthScale;

    vec2 nStep = step * 1.75;
    vec3 n0 = SampleNormal(inUV);
    vec3 nx = SampleNormal(inUV + vec2(nStep.x, 0.0));
    vec3 ny = SampleNormal(inUV + vec2(0.0, nStep.y));
    float nDiff =
        (1.0 - clamp(dot(n0, nx), 0.0, 1.0)) +
        (1.0 - clamp(dot(n0, ny), 0.0, 1.0));
    const float kNormalDeadzone = 0.08;
    float edgeNormal =
        max(nDiff - kNormalDeadzone, 0.0) * push.edgeNormalScale;

    float edge = clamp(max(edgeDepth, edgeNormal * 0.85) * push.strength,
                       0.0, 1.0);
    outColor = vec4(mix(scene, push.edgeColor.rgb, edge), 1.0);
}
