#version 450

// Final composite pass: combine opaque, translucent, and bloom
// Reads all inputs via texture samplers (cross-pass from Gbuffer pass
// and bloom pass)

layout(binding = 0) uniform sampler2D inOpaque;
layout(binding = 1) uniform sampler2D inTranslucent;
layout(binding = 2) uniform sampler2D inBloom;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

const float bloomStrength = 1.5;

void main() {
    vec4 opaqueColor  = texture(inOpaque, inTexCoord);
    vec4 transColor   = texture(inTranslucent, inTexCoord);
    vec4 bloomColor   = texture(inBloom, inTexCoord);

    vec3 finalColor = mix(opaqueColor.rgb, transColor.rgb, transColor.a);
    finalColor += bloomColor.rgb * bloomStrength;

    outColor = vec4(finalColor, 1.0);
}
