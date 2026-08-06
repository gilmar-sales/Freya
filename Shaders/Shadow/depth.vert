#version 450

layout(push_constant) uniform ShadowPushConstant {
    mat4 lightVP;
    vec4 lightPosFar; // xyz=light pos, w=far (<=0: use HW NDC depth)
    vec4 reverseZAndPad; // x = reverseZ
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 5) in mat4 inModel;

layout(location = 0) out vec3 outWorldPos;

void main() {
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    gl_Position = pc.lightVP * worldPos;
}
