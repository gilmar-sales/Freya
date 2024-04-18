#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
} pub;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 2) in vec4 inModel1;
layout(location = 3) in vec4 inModel2;
layout(location = 4) in vec4 inModel3;
layout(location = 5) in vec4 inModel4;

layout(location = 0) out vec3 fragColor;

void main() {
    mat4 model = mat4(inModel1, inModel2, inModel3, inModel4);

    gl_Position = pub.proj * pub.view * model * vec4(inPosition, 1.0);
    fragColor = inColor;
}