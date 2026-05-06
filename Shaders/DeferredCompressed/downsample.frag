#version 450

// Downsample pass using KawaseBlur technique
// Input comes from threshold output, renders to half resolution

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inThreshold;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    // Simply pass through - full resolution input will be scaled by framebuffer size
    // The half-resolution framebuffer automatically downsamples
    outColor = subpassLoad(inThreshold);
}