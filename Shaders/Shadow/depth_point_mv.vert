#version 450
#extension GL_EXT_multiview : enable

// Point cube multiview: one draw writes all 6 faces via gl_ViewIndex.

layout(set = 2, binding = 0) uniform ShadowBuffer {
    mat4 cascadeViewProj[4];
    vec4 cascadeSplits;
    vec4 params;
    mat4 spotViewProj[4];
    vec4 spotLightIndex;
    vec4 pointLightPosFar[2];
    vec4 pointLightIndex;
    vec4 reverseZ;
    vec4 pcss;
    vec4 cascadeTexelSize;
    mat4 pointFaceViewProj[12];
} shadows;

layout(push_constant) uniform ShadowPushConstant {
    mat4 lightVP;
    vec4 lightPosFar;
    vec4 reverseZAndPad; // x = reverseZ, y = point slot
} pc;

layout(std430, set = 0, binding = 0) readonly buffer BoneBuffer {
    mat4 bones[];
};
layout(std430, set = 0, binding = 1) readonly buffer PrevBoneBuffer {
    mat4 prevBones[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 4) in vec2 inTexCoord;
layout(location = 5) in mat4 inModel;
layout(location = 13) in uvec4 inInstanceIds;
layout(location = 14) in uvec4 inJoints;
layout(location = 15) in vec4 inWeights;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) flat out uint outMaterialId;

const uint kNoSkin = 0xFFFFFFFFu;

vec3 skinPosition(uint boneOffset)
{
    if (boneOffset == kNoSkin)
        return inPosition;

    vec3 pos = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
        float w = inWeights[i];
        if (w <= 0.0)
            continue;
        pos += (bones[boneOffset + inJoints[i]] * vec4(inPosition, 1.0)).xyz * w;
    }
    return pos;
}

void main() {
    vec3 localPos = skinPosition(inInstanceIds.w);
    vec4 worldPos = inModel * vec4(localPos, 1.0);
    outWorldPos = worldPos.xyz;
    outTexCoord = inTexCoord;
    outMaterialId = inInstanceIds.x;
    int faceIndex = int(pc.reverseZAndPad.y) * 6 + gl_ViewIndex;
    gl_Position = shadows.pointFaceViewProj[faceIndex] * worldPos;
}
