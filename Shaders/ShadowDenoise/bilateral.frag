#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outMask;

layout(binding = 0) uniform sampler2D noisyMask;
layout(binding = 1) uniform sampler2D sceneDepth;
layout(binding = 2) uniform sampler2D sceneNormal;

layout(push_constant) uniform DenoisePC {
    vec4 axisAndParams; // xy = blur axis, z = radius
    vec4 sigmas;        // x = depthSigma, y = normalSigma
} pc;

void main() {
    float centerMask = texture(noisyMask, inUV).r;
    float centerDepth = texture(sceneDepth, inUV).r;
    vec3 centerNormal = normalize(texture(sceneNormal, inUV).xyz);

    vec2 texel = 1.0 / vec2(textureSize(noisyMask, 0));
    vec2 axis = pc.axisAndParams.xy;
    float radius = max(pc.axisAndParams.z, 1.0);
    float depthSigma = max(pc.sigmas.x, 1e-5);
    float normalSigma = max(pc.sigmas.y, 1e-5);

    float weightSum = 1.0;
    float valueSum = centerMask;

    int iRadius = int(ceil(radius));
    for (int i = -iRadius; i <= iRadius; ++i) {
        if (i == 0)
            continue;

        float dist = float(i);
        float spatial = exp(-0.5 * (dist * dist) / (radius * radius));

        vec2 uv = inUV + axis * (dist * texel);
        float sampleDepth = texture(sceneDepth, uv).r;
        vec3 sampleNormal = normalize(texture(sceneNormal, uv).xyz);
        float sampleMask = texture(noisyMask, uv).r;

        float depthDelta = abs(sampleDepth - centerDepth);
        float depthW = exp(-0.5 * (depthDelta * depthDelta) /
                           (depthSigma * depthSigma));

        float nDot = clamp(dot(centerNormal, sampleNormal), 0.0, 1.0);
        float normalDelta = 1.0 - nDot;
        float normalW = exp(-0.5 * (normalDelta * normalDelta) *
                            (normalSigma * normalSigma));

        float w = spatial * depthW * normalW;
        weightSum += w;
        valueSum += sampleMask * w;
    }

    outMask = valueSum / max(weightSum, 1e-5);
}
