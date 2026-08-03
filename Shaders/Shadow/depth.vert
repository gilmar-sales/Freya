#version 450

layout(push_constant) uniform ShadowPushConstant {
    mat4 lightVP;
} pc;

layout (location = 0) in vec3 inPosition;
layout (location = 5) in mat4 inModel;

void main() {
    gl_Position = pc.lightVP * inModel * vec4(inPosition, 1.0);
}
