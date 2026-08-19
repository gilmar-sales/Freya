#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "Include/shadow_alpha.inc"

layout(push_constant) uniform ShadowPushConstant {
    mat4 lightVP;
    vec4 lightPosFar; // xyz=light pos, w=far
    vec4 reverseZAndPad; // x = reverseZ
} pc;

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) flat in uint inMaterialId;

void main() {
    discardMaskedShadow(inTexCoord, inMaterialId);
    float dist = length(inWorldPos - pc.lightPosFar.xyz);
    float linear = clamp(dist / pc.lightPosFar.w, 0.0, 1.0);
    gl_FragDepth =
        pc.reverseZAndPad.x > 0.5 ? (1.0 - linear) : linear;
}
