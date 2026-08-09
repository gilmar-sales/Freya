#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj; // jittered
    vec4 ambientLight;
    // Match C++ alignas(64) padding before invViewProjection.
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
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec2 inTexCoord;
layout (location = 5) in mat4 inModel;
layout (location = 9) in mat4 inPrevModel;
layout (location = 13) in uvec4 inInstanceIds; // mat, entity, flags, boneOffset
layout (location = 14) in uvec4 inJoints;
layout (location = 15) in vec4 inWeights;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec2 outTexCoord;
layout (location = 2) out mat3 outTBN;
layout (location = 5) out vec3 outColor;
layout (location = 6) out vec2 outVelocity;
layout (location = 7) flat out uint outMaterialId;

const uint kNoSkin = 0xFFFFFFFFu;

void skinPositionNormalTangent(uint boneOffset, out vec3 pos, out vec3 nrm,
                               out vec3 tan, out vec3 prevPos)
{
    if (boneOffset == kNoSkin) {
        pos = inPosition;
        nrm = inNormal;
        tan = inTangent;
        prevPos = inPosition;
        return;
    }

    mat4 skin = mat4(0.0);
    mat4 prevSkin = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        float w = inWeights[i];
        if (w <= 0.0)
            continue;
        uint ji = boneOffset + inJoints[i];
        skin += bones[ji] * w;
        prevSkin += prevBones[ji] * w;
    }
    pos = (skin * vec4(inPosition, 1.0)).xyz;
    prevPos = (prevSkin * vec4(inPosition, 1.0)).xyz;
    nrm = normalize(mat3(skin) * inNormal);
    tan = normalize(mat3(skin) * inTangent);
}

void main() {
    uint boneOffset = inInstanceIds.w;
    vec3 localPos;
    vec3 localN;
    vec3 localT;
    vec3 prevLocalPos;
    skinPositionNormalTangent(boneOffset, localPos, localN, localT, prevLocalPos);

    vec4 worldPos = inModel * vec4(localPos, 1.0);
    vec4 prevWorldPos = inPrevModel * vec4(prevLocalPos, 1.0);
    gl_Position = pub.proj * pub.view * worldPos;
    outPosition = worldPos.xyz;
    outTexCoord = inTexCoord;
    outColor = inColor;
    outMaterialId = inInstanceIds.x;

    // Motion in UV space from UNJITTERED projections only. Including the
    // Halton jitter here would contaminate velocity and cause shimmer.
    // Rasterization still uses pub.proj (jittered) via gl_Position above.
    vec4 currClip = pub.unjitteredProjection * pub.view * worldPos;
    vec4 prevClip = pub.prevViewProjection * prevWorldPos;
    vec2 currNdc = currClip.xy / max(currClip.w, 1e-5);
    vec2 prevNdc = prevClip.xy / max(prevClip.w, 1e-5);
    outVelocity = (currNdc - prevNdc) * 0.5;

    vec3 T = normalize(vec3(inModel * vec4(localT, 0.0)));
    vec3 N = normalize(vec3(inModel * vec4(localN, 0.0)));
    T = normalize(T - N * dot(N, T));
    vec3 B = cross(N, T);

    outTBN = mat3(T, B, N);
}
