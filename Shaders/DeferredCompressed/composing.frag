#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inOpaque;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inTranslucent;
layout (input_attachment_index = 2, binding = 2) uniform subpassInput inBloom;

layout (location = 0) out vec4 outColor;

const float bloomStrength = 1.5; // bloom intensity multiplier

void main() {
    vec4 opaqueColor = subpassLoad(inOpaque);
    vec4 transColor = subpassLoad(inTranslucent);
    vec4 bloomColor = subpassLoad(inBloom);

    vec3 finalColor = mix(opaqueColor.rgb, transColor.rgb, transColor.a);
    
    // Add bloom contribution
    finalColor += bloomColor.rgb * bloomStrength;
    
    outColor = vec4(finalColor, 1.0);
}
