#version 450

layout(push_constant) uniform ShadowPushConstant {
    mat4 lightVP;
    vec4 lightPosFar; // xyz=light pos, w=far
    vec4 reverseZAndPad; // x = reverseZ
} pc;

layout(location = 0) in vec3 inWorldPos;

void main() {
    float dist = length(inWorldPos - pc.lightPosFar.xyz);
    float linear = clamp(dist / pc.lightPosFar.w, 0.0, 1.0);
    gl_FragDepth =
        pc.reverseZAndPad.x > 0.5 ? (1.0 - linear) : linear;
}
