#version 450

layout(set = 0, binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj; // jittered
    vec4 ambientLight;
    vec4 _pad0;
    vec4 _pad1;
    vec4 _pad2;
    mat4 invViewProjection;
    mat4 prevViewProjection;
    mat4 unjitteredProjection;
} pub;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec2 inTexCoord;
layout (location = 5) in mat4 inModel;
layout (location = 9) in mat4 inPrevModel;
layout (location = 13) in uvec2 inMaterialEntity;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec2 outTexCoord;
layout (location = 2) out vec3 outColor;
layout (location = 3) flat out uint outMaterialId;
layout (location = 4) out float outViewZ;
layout (location = 5) out vec3 outWorldNormal;

void main() {
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    gl_Position = pub.proj * pub.view * worldPos;
    outWorldPos = worldPos.xyz;
    outTexCoord = inTexCoord;
    outColor = inColor;
    outMaterialId = inMaterialEntity.x;
    outViewZ = abs((pub.view * worldPos).z);

    mat3 normalMat = mat3(inModel);
    outWorldNormal = normalize(normalMat * inNormal);
}
