#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec3 lightDirection;
} pub;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in mat4 inModel;

layout(location = 0) out vec3 fragColor;

const float AMBIENT_LIGHT = 0.03;

void main() {
    gl_Position = pub.proj * pub.view * inModel * vec4(inPosition, 1.0);
    
    vec3 normalWorldSpace = normalize(mat3(inModel)*inNormal);

    float lightIntensity = AMBIENT_LIGHT + max(dot(inNormal, pub.lightDirection), 0);
    
    fragColor = lightIntensity * inColor;
}