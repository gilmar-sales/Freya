#version 450

// Bloom upsample pass
// Passes downsample data through for composite blending.

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inDownsample;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = subpassLoad(inDownsample);
}
