#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inDepthBuffer;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inPosition;
layout (input_attachment_index = 2, binding = 2) uniform subpassInput inNormal;
layout (input_attachment_index = 3, binding = 3) uniform subpassInput inAlbedo;

layout (location = 0) out vec4 outColor;
layout (location = 0) in vec2 inUV;

void main() {
    outColor = vec4(1.0, 0.0, 0.0, 1.0); // solid red — goes to opaque buffer
}
