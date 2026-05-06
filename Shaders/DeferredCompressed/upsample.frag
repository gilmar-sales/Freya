#version 450

// Bloom upsample pass: reads downsample input attachment (within-pass)

layout(input_attachment_index = 0, binding = 1) uniform subpassInput inDownsample;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = subpassLoad(inDownsample);
}
