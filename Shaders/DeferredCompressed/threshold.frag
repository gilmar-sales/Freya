#version 450

// Bloom threshold: extract bright pixels from HDR Scene Color.

layout(binding = 0) uniform sampler2D inSceneColor;

layout(push_constant) uniform PushConstants {
    float bloomThreshold;
    float bloomExtractScale;
} pc;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 scene = texture(inSceneColor, inTexCoord);

    float luminance = dot(scene.rgb, vec3(0.299, 0.587, 0.114));
    float extraction = max(luminance - pc.bloomThreshold, 0.0);

    outColor = vec4(scene.rgb * extraction * pc.bloomExtractScale, 1.0);
}
