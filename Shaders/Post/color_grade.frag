#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inScene;

// Procedural grade (no 3D LUT texture required).
layout(push_constant) uniform GradePush {
    float contrast;
    float saturation;
    float exposure;
    float vignette;
    vec4  lift; // rgb shadow tint, a unused
    vec4  gain; // rgb highlight tint, a unused
} push;

void main() {
    vec3 c = texture(inScene, inUV).rgb;
    c *= exp2(push.exposure);

    // Lift / gain (shadows / highlights)
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    vec3 shadowed = c + push.lift.rgb * (1.0 - clamp(luma, 0.0, 1.0));
    c = mix(shadowed, shadowed * max(push.gain.rgb, vec3(0.0)),
            clamp(luma, 0.0, 1.0));

    c = (c - 0.5) * max(push.contrast, 0.0) + 0.5;

    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(vec3(g), c, push.saturation);

    float r = length(inUV - vec2(0.5));
    float vig = 1.0 - smoothstep(0.4, 0.95, r) * clamp(push.vignette, 0.0, 1.0);
    c *= vig;

    outColor = vec4(max(c, vec3(0.0)), 1.0);
}
