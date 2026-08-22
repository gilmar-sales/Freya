#version 450
#extension GL_GOOGLE_include_directive : require

// Item highlight glow: soft colored aura around BindMaterial surfaces.
// Requires BindMaterial (mask.count > 0); otherwise passes scene through.

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inScene;
layout(set = 0, binding = 1) uniform sampler2D inDepth;

#include "Include/post_mask.inc"

layout(push_constant) uniform GlowPush {
    float intensity; // rim / outer aura strength
    float radius;    // dilation in px @ 1080p
    float fill;      // 0..1 tint over the item itself
    float reverseZ;
    vec4  color;     // rgb highlight color
} push;

bool IsSky(float depth) {
    if (push.reverseZ > 0.5)
        return depth <= 1e-6;
    return depth >= 0.999999;
}

float SampleHighlight(vec2 uv) {
    float depth = texture(inDepth, uv).r;
    if (IsSky(depth))
        return 0.0;
    return MaterialIncluded(SampleMaterialId(uv)) ? 1.0 : 0.0;
}

void main() {
    vec3 scene = texture(inScene, inUV).rgb;

    // No bound materials → not an item-highlight pass.
    if (mask.count == 0u) {
        outColor = vec4(scene, 1.0);
        return;
    }

    ivec2 size = textureSize(inScene, 0);
    vec2 texel = 1.0 / vec2(size);
    float r = max(push.radius, 1.0) * max(float(size.y) / 1080.0, 1.0);

    float center = SampleHighlight(inUV);

    // Dilate mask in a ring of taps → soft outer silhouette.
    float dilated = center;
    const vec2 offs[16] = vec2[](
        vec2( 1.0,  0.0), vec2(-1.0,  0.0),
        vec2( 0.0,  1.0), vec2( 0.0, -1.0),
        vec2( 0.707,  0.707), vec2(-0.707,  0.707),
        vec2( 0.707, -0.707), vec2(-0.707, -0.707),
        vec2( 1.0,  0.5), vec2(-1.0,  0.5),
        vec2( 0.5,  1.0), vec2( 0.5, -1.0),
        vec2(-0.5,  1.0), vec2(-0.5, -1.0),
        vec2( 1.0, -0.5), vec2(-1.0, -0.5));
    for (int i = 0; i < 16; ++i) {
        dilated = max(dilated,
                      SampleHighlight(inUV + offs[i] * texel * r));
        dilated = max(dilated,
                      SampleHighlight(inUV + offs[i] * texel * (r * 0.5)));
    }

    float rim = clamp(dilated - center, 0.0, 1.0);
    // Soft falloff: stronger on the outer edge.
    float aura = rim * rim * (3.0 - 2.0 * rim);
    float fill = center * clamp(push.fill, 0.0, 1.0);

    vec3 glow = push.color.rgb *
                (aura * max(push.intensity, 0.0) + fill * 0.35);
    outColor = vec4(scene + glow, 1.0);
}
