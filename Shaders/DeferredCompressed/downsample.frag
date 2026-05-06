#version 450

// Bloom downsample pass: reads threshold input attachment (within-pass)
// The half-resolution framebuffer provides natural downsampling.

layout(input_attachment_index = 0, binding = 1) uniform subpassInput inThreshold;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = subpassLoad(inThreshold);
}
