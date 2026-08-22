#version 450
#extension GL_GOOGLE_include_directive : require

// Mu Online–style item upgrade glow (+0 … +13).
// Classic tiers (StrategyWiki):
//   0–2  none
//   3–4  red tint
//   5–6  blue tint
//   7–8  soft base-color glow
//   9–11 strong glow (+11 white spark)
//  12–13 brighter + white/blue flash waves (the famous +13 look)
// Requires BindMaterial.

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inScene;
layout(set = 0, binding = 1) uniform sampler2D inDepth;

#include "Include/post_mask.inc"

layout(push_constant) uniform MuGlowPush {
    float time;
    float level;     // 0..13
    float intensity; // global scale
    float reverseZ;
    float radius;    // rim dilation px @ 1080p
    float waveSpeed;
    float _pad0;
    float _pad1;
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

vec3 Hsv2Rgb(float h, float s, float v) {
    vec3 p = abs(fract(h + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return v * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), s);
}

// Tier colors matching classic Mu feel.
void TierLook(float lvl, out vec3 tint, out float glowAmt, out float waveAmt,
              out float flashAmt, out float rainbow) {
    tint     = vec3(1.0);
    glowAmt  = 0.0;
    waveAmt  = 0.0;
    flashAmt = 0.0;
    rainbow  = 0.0;

    if (lvl < 2.5) {
        return;
    }
    if (lvl < 4.5) {
        // +3/+4 red tint
        tint    = vec3(1.0, 0.25, 0.18);
        glowAmt = 0.35;
        return;
    }
    if (lvl < 6.5) {
        // +5/+6 blue tint
        tint    = vec3(0.25, 0.45, 1.0);
        glowAmt = 0.45;
        return;
    }
    if (lvl < 8.5) {
        // +7/+8 soft glow
        tint    = vec3(0.85, 0.95, 1.0);
        glowAmt = 0.9;
        waveAmt = 0.35;
        return;
    }
    if (lvl < 11.5) {
        // +9/+10/+11 strong glow; +11 white spark
        tint     = vec3(0.7, 0.85, 1.0);
        glowAmt  = 1.6;
        waveAmt  = 0.7;
        flashAmt = (lvl >= 10.5) ? 0.55 : 0.15;
        return;
    }
    // +12/+13 — brighter + white/blue flash waves
    tint     = vec3(0.75, 0.9, 1.0);
    glowAmt  = 2.2;
    waveAmt  = 1.0;
    flashAmt = 1.0;
    rainbow  = (lvl >= 12.5) ? 1.0 : 0.35;
}

void main() {
    vec3 scene = texture(inScene, inUV).rgb;

    if (mask.count == 0u) {
        outColor = vec4(scene, 1.0);
        return;
    }

    float lvl = clamp(push.level, 0.0, 13.0);
    vec3 tint;
    float glowAmt, waveAmt, flashAmt, rainbow;
    TierLook(lvl, tint, glowAmt, waveAmt, flashAmt, rainbow);

    if (glowAmt < 1e-3 && waveAmt < 1e-3) {
        outColor = vec4(scene, 1.0);
        return;
    }

    ivec2 size = textureSize(inScene, 0);
    vec2 texel = 1.0 / vec2(size);
    float r = max(push.radius, 1.0) * max(float(size.y) / 1080.0, 1.0);

    float center = SampleHighlight(inUV);
    float dilated = center;
    const vec2 offs[12] = vec2[](
        vec2( 1.0,  0.0), vec2(-1.0,  0.0),
        vec2( 0.0,  1.0), vec2( 0.0, -1.0),
        vec2( 0.707,  0.707), vec2(-0.707,  0.707),
        vec2( 0.707, -0.707), vec2(-0.707, -0.707),
        vec2( 1.0,  0.5), vec2(-1.0, -0.5),
        vec2( 0.5, -1.0), vec2(-0.5,  1.0));
    for (int i = 0; i < 12; ++i) {
        dilated = max(dilated, SampleHighlight(inUV + offs[i] * texel * r));
        dilated =
            max(dilated, SampleHighlight(inUV + offs[i] * texel * (r * 0.55)));
    }

    float rim = clamp(dilated - center, 0.0, 1.0);
    float onItem = center;

    // Scrolling diagonal “energy waves” across the item (Mu +7…+13).
    float t = push.time * max(push.waveSpeed, 0.0);
    float phase = (inUV.x * 6.0 + inUV.y * 9.0) - t * 2.2;
    float wave1 = pow(0.5 + 0.5 * sin(phase), 6.0);
    float wave2 = pow(0.5 + 0.5 * sin(phase * 1.7 + 1.3), 10.0);
    float waves = mix(wave1, max(wave1, wave2), clamp(waveAmt, 0.0, 1.0));

    // Occasional white/blue flash (+11…+13).
    float flashPulse =
        pow(0.5 + 0.5 * sin(push.time * 7.5), 20.0) * flashAmt;
    vec3 flashCol = mix(vec3(0.6, 0.85, 1.0), vec3(1.0), flashPulse);

    // Rainbow shimmer for +13 (and mild for +12).
    float hue = fract(inUV.x * 1.5 + inUV.y * 0.8 + t * 0.15 + waves * 0.2);
    vec3 rainbowCol = Hsv2Rgb(hue, 0.65, 1.0);

    vec3 glowColor = mix(tint, rainbowCol, rainbow * waves);
    glowColor = mix(glowColor, flashCol, flashPulse * 0.85);

    float scale = max(push.intensity, 0.0);
    vec3 add = vec3(0.0);

    // Surface tint / glow on the item.
    add += onItem * glowColor * glowAmt * 0.22 * scale;
    // Brilliant wave bands on the surface.
    add += onItem * glowColor * waves * waveAmt * 1.35 * scale;
    // Outer silhouette aura.
    add += rim * glowColor * (0.8 + glowAmt * 0.35) * scale;
    // Flash boost on rim for high +.
    add += rim * flashCol * flashPulse * 1.8 * scale;

    outColor = vec4(scene + add, 1.0);
}
