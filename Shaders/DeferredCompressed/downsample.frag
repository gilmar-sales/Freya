#version 450

// Bloom downsample pass with Kawase blur
// Samples 4 neighboring pixels at half-step offsets and averages them.
// The input is read via a texture sampler (layout transitions are handled
// by the subpass's input attachment reference).

layout(binding = 0) uniform sampler2D inThreshold;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = inTexCoord;
    vec2 texelSize = 1.0 / textureSize(inThreshold, 0);

    // Kawase pattern: sample 4 corners at half-pixel offsets
    vec4 s0 = texture(inThreshold, uv + vec2(-1.0, -1.0) * texelSize);
    vec4 s1 = texture(inThreshold, uv + vec2( 1.0, -1.0) * texelSize);
    vec4 s2 = texture(inThreshold, uv + vec2(-1.0,  1.0) * texelSize);
    vec4 s3 = texture(inThreshold, uv + vec2( 1.0,  1.0) * texelSize);

    outColor = (s0 + s1 + s2 + s3) * 0.25;
}
