#version 450

// Bloom threshold pass: extract bright pixels from emissive buffer
// Reads emissive via texture sampler (cross-pass from Gbuffer pass)

layout(binding = 0) uniform sampler2D inEmissive;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

const float threshold = 0.01; // Low for testing; increase for production

void main() {
    vec4 emissive = texture(inEmissive, inTexCoord);

    float luminance = dot(emissive.rgb, vec3(0.299, 0.587, 0.114));
    float extraction = max(luminance - threshold, 0.0);

    outColor = vec4(emissive.rgb * extraction, 1.0);
}
