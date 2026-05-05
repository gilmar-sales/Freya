#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inOpaque;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inTranslucent;

layout (location = 0) out vec4 outColor;

layout (location = 0) in vec2 inUV;

void main() {
    vec4 opaqueColor = subpassLoad(inOpaque);
    outColor = vec4(opaqueColor.rgb, 1.0);
}
