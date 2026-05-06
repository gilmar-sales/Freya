#version 450

// Bloom upsample pass with Kawase blur
// Samples 4 neighboring pixels at half-step offsets plus center and blends.
// The input is read via a texture sampler (layout transitions are handled
// by the subpass's input attachment reference).

layout(binding = 0) uniform sampler2D inDownsample;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = inTexCoord;
    vec2 texelSize = 1.0 / textureSize(inDownsample, 0);

    // Kawase pattern: sample 4 corners + center
    vec4 s0 = texture(inDownsample, uv + vec2(-1.0, -1.0) * texelSize);
    vec4 s1 = texture(inDownsample, uv + vec2( 1.0, -1.0) * texelSize);
    vec4 s2 = texture(inDownsample, uv + vec2(-1.0,  1.0) * texelSize);
    vec4 s3 = texture(inDownsample, uv + vec2( 1.0,  1.0) * texelSize);
    vec4 center = texture(inDownsample, uv);

    outColor = (s0 + s1 + s2 + s3 + center) * 0.2;
}
