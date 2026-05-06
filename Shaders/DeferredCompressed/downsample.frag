#version 450

// Bloom downsample pass
// Passes threshold data through for composite blending.
// The bloom spread effect is achieved by framebuffer resolution scaling
// and bilinear filtering during the final composite pass.

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inThreshold;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = subpassLoad(inThreshold);
}
