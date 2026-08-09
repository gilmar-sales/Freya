#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

layout(std430, set = 1, binding = 0) readonly buffer BoneBuffer {
    mat4 bones[];
};
layout(std430, set = 1, binding = 1) readonly buffer PrevBoneBuffer {
    mat4 prevBones[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 5) in mat4 inModel;
layout(location = 13) in uvec4 inInstanceIds;
layout(location = 14) in uvec4 inJoints;
layout(location = 15) in vec4 inWeights;

layout(location = 0) flat out uint vEntityId;

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
    gl_Position = pub.proj * pub.view * worldPos;
    vEntityId = inInstanceIds.y;
}
