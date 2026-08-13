#version 450

// WBOIT resolve over opaque HDR, then hand off to Bloom/Composite.

layout (binding = 0) uniform sampler2D inOpaque;
layout (binding = 1) uniform sampler2D inAccum;
layout (binding = 2) uniform sampler2D inReveal;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

void main() {
    vec3 opaque = texture(inOpaque, inUV).rgb;
    vec4 accum = texture(inAccum, inUV);
    float reveal = texture(inReveal, inUV).r;

    vec3 glass = accum.rgb / max(accum.a, 1e-5);
    float alpha = 1.0 - clamp(reveal, 0.0, 1.0);

    outColor = vec4(glass * alpha + opaque * (1.0 - alpha), 1.0);
}
