#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
    vec4 _pad0;
    vec4 _pad1;
    vec4 _pad2;
    mat4 invViewProjection;
    mat4 prevViewProjection;
    mat4 unjitteredProjection;
} pub;

layout(std430, set = 2, binding = 0) readonly buffer BoneBuffer {
    mat4 bones[];
};
layout(std430, set = 2, binding = 1) readonly buffer PrevBoneBuffer {
    mat4 prevBones[];
};

layout (location = 0) in vec3 inPosition;
layout (location = 5) in mat4 inModel;
layout (location = 13) in uvec4 inInstanceIds;
layout (location = 14) in uvec4 inJoints;
layout (location = 15) in vec4 inWeights;

const uint kNoSkin = 0xFFFFFFFFu;

// Must match gbuffer.vert position skinning exactly (same ops / order) so the
// depth prepass and G-buffer produce bit-identical clip depths.
vec3 skinPosition(uint boneOffset)
{
    if (boneOffset == kNoSkin)
        return inPosition;

    mat4 skin = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        float w = inWeights[i];
        if (w <= 0.0)
            continue;
        skin += bones[boneOffset + inJoints[i]] * w;
    }
    return (skin * vec4(inPosition, 1.0)).xyz;
}

void main() {
    vec3 localPos = skinPosition(inInstanceIds.w);
    vec4 worldPos = inModel * vec4(localPos, 1.0);
    gl_Position = pub.proj * pub.view * worldPos;
}
