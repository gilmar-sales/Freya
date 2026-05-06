#version 450

// Threshold pass: extracts pixels above luminance threshold for bloom
// This works on the emissive attachment which already contains only emissive values

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inEmissive;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

const float threshold = 0.8; // luminance threshold for bloom extraction

void main() {
    vec4 emissive = subpassLoad(inEmissive);
    
    // Calculate luminance
    float luminance = dot(emissive.rgb, vec3(0.299, 0.587, 0.114));
    
    // Extract only bright pixels for bloom
    float extraction = max(luminance - threshold, 0.0);
    
    outColor = vec4(emissive.rgb * extraction, 1.0);
}