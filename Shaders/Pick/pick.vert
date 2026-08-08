#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

layout(location = 0) in vec3 inPosition;
layout(location = 5) in mat4 inModel;
layout(location = 13) in uvec2 inMaterialEntity;

layout(location = 0) flat out uint vEntityId;

void main() {
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    gl_Position = pub.proj * pub.view * worldPos;
    vEntityId = inMaterialEntity.y;
}
