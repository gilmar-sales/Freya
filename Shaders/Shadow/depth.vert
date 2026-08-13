#version 450

layout(push_constant) uniform ShadowPushConstant {
    mat4 lightVP;
    vec4 lightPosFar; // xyz=light pos, w=far (<=0: use HW NDC depth)
    vec4 reverseZAndPad; // x = reverseZ
} pc;

layout(std430, set = 0, binding = 0) readonly buffer BoneBuffer {
    mat4 bones[];
};
layout(std430, set = 0, binding = 1) readonly buffer PrevBoneBuffer {
    mat4 prevBones[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 5) in mat4 inModel;
layout(location = 13) in uvec4 inInstanceIds;
layout(location = 14) in uvec4 inJoints;
layout(location = 15) in vec4 inWeights;

layout(location = 0) out vec3 outWorldPos;

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
    gl_Position = pc.lightVP * worldPos;
}
