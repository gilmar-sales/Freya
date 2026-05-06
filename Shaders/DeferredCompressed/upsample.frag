#version 450

// Upsample pass using KawaseBlur technique
// Input comes from downsample output, renders back to half resolution

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inDownsample;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    // Pass through - upsample will be done by bilinear sampling when composite reads
    // from half-res bloomUp to full-res bloomResult via framebuffer scaling
    outColor = subpassLoad(inDownsample);
}