#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inOpaque;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inTranslucent;

layout (location = 0) out vec4 outColor;

void main() {
    vec4 opaqueColor = subpassLoad(inOpaque);
    vec4 transColor = subpassLoad(inTranslucent);

    vec3 finalColor = mix(opaqueColor.rgb, transColor.rgb, transColor.a);
    outColor = vec4(finalColor, 1.0);
}
