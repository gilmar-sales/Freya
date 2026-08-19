#ifndef SHADOWS_INC
#define SHADOWS_INC

// Requires UBOs `lights` and `shadows`, plus cascade/spot/point shadow
// samplers declared by the including shader.

const vec2 kPoissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790));

int SoftShadowTapCount() {
    return clamp(int(shadows.pcss.w + 0.5), 1, 16);
}

float ShadowMapResolution() {
    return max(shadows.reverseZ.y, 1.0);
}

float SoftUvRadius(float texelWorld, float maxTexels) {
    float ts = max(texelWorld, 1e-6);
    float softTexels =
        clamp(shadows.pcss.x / ts, 1.0, min(shadows.pcss.y, maxTexels));
    return softTexels / ShadowMapResolution();
}

void ShadowTangentBasis(vec3 axis, out vec3 T, out vec3 B) {
    vec3 up = abs(axis.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    T = normalize(cross(up, axis));
    B = cross(axis, T);
}

float SoftShadow2DUV(sampler2DArrayShadow map, mat4 lightVP, vec3 biased,
                     float layer, float depthBiasAmount, float uvRadius) {
    vec4 centerClip = lightVP * vec4(biased, 1.0);
    vec3 centerNdc = centerClip.xyz / centerClip.w;
    if (abs(centerNdc.x) > 1.0 || abs(centerNdc.y) > 1.0)
        return 1.0;

    vec2 uv = centerNdc.xy * 0.5 + 0.5;
    bool reverseZ = shadows.reverseZ.x > 0.5;
    float depthRef = reverseZ ? (centerNdc.z + depthBiasAmount)
                              : (centerNdc.z - depthBiasAmount);

    int tapCount = SoftShadowTapCount();
    float result = 0.0;
    for (int i = 0; i < 16; ++i) {
        if (i >= tapCount)
            break;
        vec2 sampleUv = uv + kPoissonDisk[i] * uvRadius;
        if (sampleUv.x < 0.0 || sampleUv.x > 1.0 || sampleUv.y < 0.0 ||
            sampleUv.y > 1.0) {
            result += 1.0;
            continue;
        }
        result += texture(map, vec4(sampleUv, layer, depthRef));
    }
    return result / float(tapCount);
}

float SampleCascadeShadow(vec3 worldPos, vec3 N, vec3 L) {
    int cascadeCount = int(shadows.params.z);
    if (cascadeCount < 1)
        return 1.0;

    float viewDepth = dot(worldPos - lights.viewPosition.xyz,
                          lights.cameraForward.xyz);

    int cascade = cascadeCount - 1;
    for (int i = 0; i < cascadeCount; ++i) {
        if (viewDepth < shadows.cascadeSplits[i]) {
            cascade = i;
            break;
        }
    }

    float nDotL = max(dot(N, normalize(L)), 0.0);
    float slope = sqrt(max(1.0 - nDotL * nDotL, 0.0));
    vec3 lightDir = normalize(L);

    float ts = max(shadows.cascadeTexelSize[cascade], 1e-6);
    float worldBias = ts * (3.0 + 5.0 * slope) * max(shadows.params.y, 1.0);
    vec3 biased =
        worldPos + N * (worldBias * 0.9) + lightDir * (worldBias * 0.35);

    vec4 clip = shadows.cascadeViewProj[cascade] * vec4(biased, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0)
        return 1.0;

    vec4 clipUnbiased = shadows.cascadeViewProj[cascade] * vec4(worldPos, 1.0);
    float ndcBias =
        abs(ndc.z - clipUnbiased.z / max(abs(clipUnbiased.w), 1e-6));
    float depthBiasAmount =
        max(shadows.params.x * (3.0 + 4.5 * slope), ndcBias * 1.15);

    float uvRadius =
        max(SoftUvRadius(ts, shadows.pcss.y), 1.0 / ShadowMapResolution());

    float shadow = SoftShadow2DUV(cascadeShadowMap,
                                  shadows.cascadeViewProj[cascade], biased,
                                  float(cascade), depthBiasAmount, uvRadius);

    if (cascade < cascadeCount - 1) {
        float splitNear =
            (cascade == 0) ? 0.0 : shadows.cascadeSplits[cascade - 1];
        float splitFar = shadows.cascadeSplits[cascade];
        float blendStart = mix(splitNear, splitFar, 0.9);
        if (viewDepth > blendStart) {
            int nextCascade = cascade + 1;
            float tsNext = max(shadows.cascadeTexelSize[nextCascade], 1e-6);
            float worldBiasNext =
                tsNext * (3.0 + 5.0 * slope) * max(shadows.params.y, 1.0);
            vec3 biasedNext = worldPos + N * (worldBiasNext * 0.9) +
                              lightDir * (worldBiasNext * 0.35);

            vec4 clipNext =
                shadows.cascadeViewProj[nextCascade] * vec4(biasedNext, 1.0);
            vec3 ndcNext = clipNext.xyz / clipNext.w;
            vec4 clipUnbiasedNext =
                shadows.cascadeViewProj[nextCascade] * vec4(worldPos, 1.0);
            float ndcBiasNext = abs(
                ndcNext.z -
                clipUnbiasedNext.z / max(abs(clipUnbiasedNext.w), 1e-6));
            float depthBiasNext =
                max(shadows.params.x * (3.0 + 4.5 * slope), ndcBiasNext * 1.15);

            float next = SoftShadow2DUV(
                cascadeShadowMap, shadows.cascadeViewProj[nextCascade],
                biasedNext, float(nextCascade), depthBiasNext, uvRadius);
            float t = smoothstep(blendStart, splitFar, viewDepth);
            shadow = mix(shadow, next, t);
        }
    }
    return shadow;
}

float ShadowDistanceFade(float dist, float far) {
    float fadeStart = max(far * 0.5, far - 8.0);
    return 1.0 - smoothstep(fadeStart, far, dist);
}

float SampleSpotShadow(int lightIndex, vec3 worldPos, vec3 N, vec3 L) {
    int slot = -1;
    for (int s = 0; s < 4; ++s) {
        if (int(shadows.spotLightIndex[s]) == lightIndex) {
            slot = s;
            break;
        }
    }
    if (slot < 0)
        return 1.0;

    vec3 lightPos = lights.lightPositions[lightIndex].xyz;
    vec3 aim = normalize(lights.lightDirectionsAndCutoff[lightIndex].xyz);
    float outer = lights.lightOuterCutoffAndIntensity[lightIndex].x;
    vec3 fromLight = worldPos - lightPos;
    float distHint = length(fromLight);
    if (distHint < 1e-5)
        return 1.0;
    if (dot(fromLight / distHint, aim) < outer)
        return 1.0;

    float radius = lights.lightColorsAndRadius[lightIndex].w;
    float fade = ShadowDistanceFade(distHint, radius);
    if (fade <= 0.0)
        return 1.0;

    float nDotL = max(dot(N, normalize(L)), 0.0);
    float slope = sqrt(max(1.0 - nDotL * nDotL, 0.0));
    float texelWorld = (2.0 * distHint) / ShadowMapResolution();
    float normalScale = texelWorld * shadows.params.y * (0.5 + 1.5 * slope);
    float lightPush = texelWorld * (1.0 + slope);
    vec3 biased = worldPos + N * normalScale + normalize(L) * lightPush;
    float depthBiasAmount = shadows.params.x * (1.0 + 1.5 * slope);

    return mix(1.0,
               SoftShadow2DUV(spotShadowMap, shadows.spotViewProj[slot],
                              biased, float(slot), depthBiasAmount,
                              SoftUvRadius(texelWorld, shadows.pcss.y)),
               fade);
}

float SamplePointShadow(int lightIndex, vec3 worldPos, vec3 N) {
    int slot = -1;
    for (int s = 0; s < 2; ++s) {
        if (int(shadows.pointLightIndex[s]) == lightIndex) {
            slot = s;
            break;
        }
    }
    if (slot < 0)
        return 1.0;

    vec3 lightPos = shadows.pointLightPosFar[slot].xyz;
    float far = shadows.pointLightPosFar[slot].w;

    vec3 toLight = lightPos - worldPos;
    float distToLight = length(toLight);
    if (distToLight < 1e-5 || distToLight >= far)
        return 1.0;

    vec3 L = toLight / distToLight;
    float nDotL = max(dot(N, L), 0.0);
    float slope = sqrt(max(1.0 - nDotL * nDotL, 0.0));
    bool reverseZ = shadows.reverseZ.x > 0.5;
    float texelWorld = (2.0 * distToLight) / ShadowMapResolution();
    float normalScale = texelWorld * shadows.params.y * (0.5 + 1.5 * slope);
    float lightPush = texelWorld * (1.0 + slope);
    vec3 biased = worldPos + N * normalScale + L * lightPush;
    vec3 dir = biased - lightPos;
    float dist = length(dir);
    if (dist >= far)
        return 1.0;

    float linear = dist / far;
    float encoded = reverseZ ? (1.0 - linear) : linear;
    float depthBias = shadows.params.x * (1.0 + 1.5 * slope);
    float depthRef = reverseZ ? (encoded + depthBias) : (encoded - depthBias);

    vec3 Ndir = normalize(dir);
    vec3 T, B;
    ShadowTangentBasis(Ndir, T, B);
    float softAngle =
        clamp(shadows.pcss.x / max(dist, 0.1), 0.002, 0.015);
    int tapCount = SoftShadowTapCount();
    float result = 0.0;
    for (int i = 0; i < 16; ++i) {
        if (i >= tapCount)
            break;
        vec2 o = kPoissonDisk[i] * softAngle;
        vec3 sampleDir = normalize(Ndir + T * o.x + B * o.y);
        result += texture(pointShadowMap, vec4(sampleDir, float(slot)),
                          depthRef);
    }
    float fade = ShadowDistanceFade(distToLight, far);
    return mix(1.0, result / float(tapCount), fade);
}

float GetShadowFactor(int i, float lightType, vec3 worldPos, vec3 N,
                      vec3 L, bool receiveShadow) {
    if (!receiveShadow || lights.lightOuterCutoffAndIntensity[i].w < 0.5)
        return 1.0;
    float shadow = 1.0;
    if (lightType >= 0.5 && lightType < 1.5)
        shadow = SampleCascadeShadow(worldPos, N, L);
    else if (lightType >= 1.5 && lightType < 2.5)
        shadow = SampleSpotShadow(i, worldPos, N, L);
    else if (lightType < 0.5)
        shadow = SamplePointShadow(i, worldPos, N);
    return mix(shadows.pcss.z, 1.0, shadow);
}

#endif
