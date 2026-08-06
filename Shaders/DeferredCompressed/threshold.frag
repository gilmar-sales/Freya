#version 450

// Bloom threshold: extract bright pixels from HDR Scene Color.

layout(binding = 0) uniform sampler2D inSceneColor;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

const float bloomThreshold = 0.75;
const float bloomExtractScale = 1.0;

void main() {
    vec4 scene = texture(inSceneColor, inTexCoord);

    float luminance = dot(scene.rgb, vec3(0.299, 0.587, 0.114));
    float extraction = max(luminance - bloomThreshold, 0.0);

    outColor = vec4(scene.rgb * extraction * bloomExtractScale, 1.0);
}
