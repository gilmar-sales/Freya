#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outMask;

layout(binding = 0) uniform sampler2D halfMask;
layout(binding = 1) uniform sampler2D halfDepth;
layout(binding = 2) uniform sampler2D halfNormal;
layout(binding = 3) uniform sampler2D fullDepth;
layout(binding = 4) uniform sampler2D fullNormal;

layout(push_constant) uniform DenoisePC {
    vec4 axisAndParams;
    vec4 sigmas; // x = depthSigma, y = normalSigma
} pc;

void main() {
    float centerDepth = texture(fullDepth, inUV).r;
    vec3 centerNormal = normalize(texture(fullNormal, inUV).xyz);

    vec2 halfSize = vec2(textureSize(halfMask, 0));

    vec2 base = inUV * halfSize - 0.5;
    vec2 f = fract(base);
    ivec2 i0 = ivec2(floor(base));

    float depthSigma = max(pc.sigmas.x, 1e-5);
    float normalSigma = max(pc.sigmas.y, 1e-5);

    float weightSum = 0.0;
    float valueSum = 0.0;

    for (int dy = 0; dy <= 1; ++dy) {
        for (int dx = 0; dx <= 1; ++dx) {
            ivec2 ic =
                clamp(i0 + ivec2(dx, dy), ivec2(0), ivec2(halfSize) - 1);
            vec2 uv = (vec2(ic) + 0.5) / halfSize;

            float sampleDepth = texture(halfDepth, uv).r;
            vec3 sampleNormal = normalize(texture(halfNormal, uv).xyz);
            float sampleMask = texture(halfMask, uv).r;

            float bilinear = ((dx == 0) ? (1.0 - f.x) : f.x) *
                             ((dy == 0) ? (1.0 - f.y) : f.y);

            float depthDelta = abs(sampleDepth - centerDepth);
            float depthW = exp(-0.5 * (depthDelta * depthDelta) /
                               (depthSigma * depthSigma));

            float nDot = clamp(dot(centerNormal, sampleNormal), 0.0, 1.0);
            float normalDelta = 1.0 - nDot;
            float normalW = exp(-0.5 * (normalDelta * normalDelta) *
                                (normalSigma * normalSigma));

            float w = max(bilinear, 1e-4) * depthW * normalW;
            weightSum += w;
            valueSum += sampleMask * w;
        }
    }

    outMask = valueSum / max(weightSum, 1e-5);
}
