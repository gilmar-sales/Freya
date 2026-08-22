#version 450

// Soft-knee bright extract from HDR scene color.

layout(binding = 0) uniform sampler2D inSceneColor;

layout(push_constant) uniform PushConstants {
    float bloomThreshold;
    float bloomExtractScale;
} pc;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 scene = texture(inSceneColor, inTexCoord).rgb;

    float luminance = dot(scene, vec3(0.2126, 0.7152, 0.0722));
    float threshold = max(pc.bloomThreshold, 1e-4);
    float knee = threshold * 0.5;
    float soft = luminance - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee + 1e-4);
    float contribution = max(soft, luminance - threshold);
    contribution /= max(luminance, 1e-4);

    outColor = vec4(scene * contribution * pc.bloomExtractScale, 1.0);
}
