#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in mat4 inModel;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosition;
layout(location = 2) out vec3 fragNormal;

void main() {
    vec4 vertexPosition = inModel * vec4(inPosition, 1.0);
    gl_Position = pub.proj * pub.view * vertexPosition;
    
    fragNormal = normalize(mat3(inModel)*inNormal);
    fragPosition = vertexPosition.xyz;
    fragColor = inColor;
}