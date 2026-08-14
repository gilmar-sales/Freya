#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D inDepth;
layout(set = 0, binding = 1) uniform sampler2D inAlbedo;
layout(set = 0, binding = 2) uniform sampler2D inNormal;
layout(set = 0, binding = 3) uniform sampler2D inPbr;

layout(set = 0, binding = 4) uniform CameraBuffer {
    mat4 view;
    mat4 projection;
    vec4 ambientLight;
    // Match C++ alignas(64) between ambient and invViewProjection.
    vec4 _pad0;
    vec4 _pad1;
    vec4 _pad2;
    mat4 invViewProjection;
} camera;

layout(set = 0, binding = 5) uniform LightBuffer {
    vec4 lightPositions[16];
    vec4 lightColorsAndRadius[16];
    vec4 lightDirectionsAndCutoff[16];
    vec4 lightOuterCutoffAndIntensity[16];
    vec4 lightAreaTangents[16];
    vec4 viewPosition;
    vec4 cameraForward;
    uint lightCount;
    float iblIntensity;
    float exposure;
} lights;

layout(set = 0, binding = 6) uniform sampler2D irradianceMap;
layout(set = 0, binding = 7) uniform sampler2D prefilterMap;
layout(set = 0, binding = 8) uniform sampler2D brdfLUT;
layout(set = 0, binding = 9) uniform sampler2D ltcMatrixMap;
layout(set = 0, binding = 10) uniform sampler2D ltcAmplMap;

layout(set = 0, binding = 11) uniform ShadowBuffer {
    mat4 cascadeViewProj[4];
    vec4 cascadeSplits;
    vec4 params; // x=bias y=normalBias(texels) z=cascadeCount w=softScale
    mat4 spotViewProj[4];
    vec4 spotLightIndex;
    vec4 pointLightPosFar[2];
    vec4 pointLightIndex;
    vec4 reverseZ; // x=Reverse-Z, y=shadow map resolution
    vec4 pcss; // x=lightSize(world) y=maxSoft(texels) z=minVis w=tapCount
    vec4 cascadeTexelSize; // world-space texel size per cascade
} shadows;

layout(set = 0, binding = 12) uniform sampler2DArrayShadow cascadeShadowMap;
layout(set = 0, binding = 13) uniform sampler2DArrayShadow spotShadowMap;
layout(set = 0, binding = 14) uniform samplerCubeArrayShadow pointShadowMap;
layout(set = 0, binding = 15) uniform sampler2D inSsao;

struct MaterialGPU {
    uint albedoIndex;
    uint normalIndex;
    uint roughnessIndex;
    uint emissiveIndex;
    uint metalnessIndex;
    uint alphaMode;
    float clearcoat;
    float clearcoatRoughness;
    vec4 albedoFactor;
    vec4 emissiveFactor;
    vec2 roughMetal;
    float materialId;
    float alphaCutoff;
};

layout(std430, set = 1, binding = 1) readonly buffer MaterialBuffer {
    MaterialGPU materials[];
};

layout(push_constant) uniform LightingPush {
    uint debugMode; // 0 = lit, 1 = SSAO, 2 = shadow factor
    uint _pad0;
    uint _pad1;
    uint _pad2;
} push;

layout(location = 0) out vec4 outColor;

const uint kFlagReceiveShadow = 1u;
const uint kFlagUnlit = 3u;

const float PI = 3.14159265359;
const float LUT_SIZE = 64.0;
const float LUT_SCALE = (LUT_SIZE - 1.0) / LUT_SIZE;
const float LUT_BIAS = 0.5 / LUT_SIZE;

vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
    uv *= vec2(0.15915494309, 0.31830988618);
    uv += 0.5;
    return uv;
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 1e-4);
}

float GeometrySchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 1e-4);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) *
           GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
                    pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

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

// Soft radius in UV: world light size / cascade texel → texels, clamped.
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

// Stable UV-space Poisson PCF (no per-pixel rotation → no grain without TAA).
// Out-of-bounds taps must count as lit — clamp-to-border depth is far/empty
// and would otherwise paint soft penumbra as grainy self-shadow (especially
// on cascade AABB edges).
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
    // Receiver bias in world units from cascade density, then convert to
    // light NDC via lightVP (ortho Z range varies wildly per cascade).
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
    // Constant floor + measured world→NDC bias (params.x still scales).
    float depthBiasAmount =
        max(shadows.params.x * (3.0 + 4.5 * slope), ndcBias * 1.15);

    // Soft PCF radius from the configured light size (pcss.x/y), floored at
    // one texel. Spreading the Poisson taps blurs isolated map-texel speckle
    // (which reads as black dots) into smooth penumbra instead of hard dots.
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
    // Point maps: offset ~texel at this distance (cube face spans ~2*dist).
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
    // Angular soft disk from world light size at this distance (no UV map).
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
    // Umbra floor: stacked soft occluders (two planes, etc.) don't crush to
    // pure black.
    return mix(shadows.pcss.z, 1.0, shadow);
}

vec3 IntegrateEdgeVec(vec3 v1, vec3 v2) {
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta =
        (x > 0.0) ? v
                  : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

    return cross(v1, v2) * theta_sintheta;
}

float LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4],
                   bool twoSided) {
    vec3 T1 = normalize(V - N * dot(V, N));
    vec3 T2 = cross(N, T1);
    Minv = Minv * transpose(mat3(T1, T2, N));

    vec3 L[4];
    L[0] = Minv * (points[0] - P);
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);

    vec3 dir = points[0] - P;
    vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);

    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    vec3 vsum = IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    float len = length(vsum);
    float z = vsum.z / max(len, 1e-6);
    if (behind)
        z = -z;

    vec2 uv = vec2(z * 0.5 + 0.5, len);
    uv = uv * LUT_SCALE + LUT_BIAS;

    float scale = texture(ltcAmplMap, uv).w;
    float sum = len * scale;
    if (!behind && !twoSided)
        sum = 0.0;

    return max(sum, 0.0);
}

vec3 calculateAreaLight(vec3 center, vec3 lightColor, vec3 normal,
                        vec3 tangent, float halfWidth, float halfHeight,
                        float intensity, vec3 worldPos, vec3 N, vec3 V,
                        vec3 albedo, float roughness, float metalness,
                        vec3 F0, float coatSpecAtten) {
    vec3 Nn = normalize(normal);
    vec3 T = normalize(tangent - Nn * dot(tangent, Nn));
    vec3 B = cross(Nn, T);

    vec3 points[4];
    points[0] = center - T * halfWidth - B * halfHeight;
    points[1] = center + T * halfWidth - B * halfHeight;
    points[2] = center + T * halfWidth + B * halfHeight;
    points[3] = center - T * halfWidth + B * halfHeight;

    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    vec2 uv = vec2(roughness, sqrt(1.0 - NdotV));
    uv = uv * LUT_SCALE + LUT_BIAS;

    vec4 t1 = texture(ltcMatrixMap, uv);
    vec4 t2 = texture(ltcAmplMap, uv);

    mat3 Minv = mat3(vec3(t1.x, 0.0, t1.y),
                     vec3(0.0, 1.0, 0.0),
                     vec3(t1.z, 0.0, t1.w));

    float spec = LTC_Evaluate(N, V, worldPos, Minv, points, true);
    float diff =
        LTC_Evaluate(N, V, worldPos, mat3(1.0), points, true);

    vec3 specular = F0 * t2.x + (1.0 - F0) * t2.y;
    specular *= spec * coatSpecAtten;
    vec3 diffuse = albedo * (1.0 - metalness) * diff;

    return lightColor * intensity * (diffuse + specular) / (2.0 * PI);
}

void resolveLight(vec3 lightPos, float lightType, float radius, vec3 lightDir,
                  float innerCutoff, float outerCutoff, vec3 worldPos,
                  out vec3 L, out float attenuation) {
    L = vec3(0.0);
    attenuation = 1.0;

    if (lightType < 0.5) {
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        L = normalize(toLight);
        attenuation = radius / (dist * dist + 1.0);
    } else if (lightType < 1.5) {
        L = -normalize(lightDir);
        attenuation = 1.0;
    } else {
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        L = normalize(toLight);

        float spotCos = dot(L, -normalize(lightDir));
        float spotFactor = smoothstep(outerCutoff, innerCutoff, spotCos);
        attenuation = spotFactor * radius / (dist * dist + 1.0);
    }
}

vec3 calculateLight(int lightIndex, vec3 lightPos, float lightType,
                    vec3 lightColor, float radius, vec3 lightDir,
                    float innerCutoff, float outerCutoff, float intensity,
                    vec3 worldPos, vec3 N, vec3 V, vec3 albedo,
                    float roughness, float metalness, vec3 F0,
                    bool receiveShadow, float coatSpecAtten) {
    vec3 L;
    float attenuation;
    resolveLight(lightPos, lightType, radius, lightDir, innerCutoff,
                 outerCutoff, worldPos, L, attenuation);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0 || attenuation <= 0.0) {
        return vec3(0.0);
    }

    float shadowFactor =
        GetShadowFactor(lightIndex, lightType, worldPos, N, L, receiveShadow);

    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular =
        (NDF * G * F) /
        max(4.0 * max(dot(N, V), 0.0) * NdotL, 1e-4);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);

    vec3 radiance = lightColor * intensity * attenuation;
    return (kD * albedo / PI + specular * coatSpecAtten) * radiance * NdotL *
           shadowFactor;
}

vec3 calculateClearcoatLight(vec3 lightPos, float lightType, vec3 lightColor,
                             float radius, vec3 lightDir, float innerCutoff,
                             float outerCutoff, float intensity, vec3 worldPos,
                             vec3 N, vec3 V, float coatRoughness,
                             float clearcoat) {
    vec3 L;
    float attenuation;
    resolveLight(lightPos, lightType, radius, lightDir, innerCutoff,
                 outerCutoff, worldPos, L, attenuation);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0 || attenuation <= 0.0 || clearcoat < 1e-3)
        return vec3(0.0);

    vec3 H = normalize(V + L);
    vec3 F0c = vec3(0.04);
    float NDF = DistributionGGX(N, H, coatRoughness);
    float G = GeometrySmith(N, V, L, coatRoughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0c);
    vec3 specular =
        (NDF * G * F) /
        max(4.0 * max(dot(N, V), 0.0) * NdotL, 1e-4);
    vec3 radiance = lightColor * intensity * attenuation;
    return specular * radiance * NdotL * clearcoat;
}

vec3 calculateIBL(vec3 N, vec3 V, vec3 albedo, float roughness, float metalness,
                  vec3 F0, float coatSpecAtten) {
    float NdotV = max(dot(N, V), 0.0);
    vec3 R = reflect(-V, N);

    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metalness);

    vec3 irradiance = texture(irradianceMap, SampleSphericalMap(N)).rgb;
    vec3 diffuse = irradiance * albedo;

    float maxLod = float(textureQueryLevels(prefilterMap) - 1);
    vec3 prefilteredColor =
        textureLod(prefilterMap, SampleSphericalMap(R), roughness * maxLod)
            .rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y) * coatSpecAtten;

    return (kD * diffuse + specular) * lights.iblIntensity;
}

vec3 calculateClearcoatIBL(vec3 N, vec3 V, float coatRoughness,
                           float clearcoat) {
    if (clearcoat < 1e-3)
        return vec3(0.0);
    float NdotV = max(dot(N, V), 0.0);
    vec3 R = reflect(-V, N);
    vec3 F0c = vec3(0.04);
    vec3 F = fresnelSchlickRoughness(NdotV, F0c, coatRoughness);
    float maxLod = float(textureQueryLevels(prefilterMap) - 1);
    vec3 prefilteredColor =
        textureLod(prefilterMap, SampleSphericalMap(R),
                   coatRoughness * maxLod)
            .rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, coatRoughness)).rg;
    return prefilteredColor * (F * brdf.x + brdf.y) * clearcoat *
           lights.iblIntensity;
}


vec3 srgbToLinear(vec3 c) {
    return pow(c, vec3(2.2));
}

uint UnpackFlags(float a) {
    return uint(clamp(round(a * 3.0), 0.0, 3.0));
}

vec3 ReconstructWorldPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = camera.invViewProjection * clip;
    return world.xyz / world.w;
}

void main() {
    float depth = texture(inDepth, inUV).r;
    bool reverseZ = shadows.reverseZ.x > 0.5;
    float ssao = texture(inSsao, inUV).r;

    // Grayscale AO debug: sky stays white; no ACES (composite tonemap off).
    if (push.debugMode != 0u) {
        bool sky = reverseZ ? (depth <= 1e-6) : (depth >= 0.999999);
        if (push.debugMode == 1u) {
            float ao = sky ? 1.0 : ssao;
            outColor = vec4(vec3(ao), 0.0);
            return;
        }
        if (sky) {
            outColor = vec4(1.0, 1.0, 1.0, 0.0);
            return;
        }
        vec3 fragPos = ReconstructWorldPos(inUV, depth);
        vec3 N = normalize(texture(inNormal, inUV).rgb * 2.0 - 1.0);
        uint flags = UnpackFlags(texture(inNormal, inUV).a);
        bool receiveShadow = (flags == kFlagReceiveShadow);
        float minShadow = 1.0;
        for (int i = 0; i < int(lights.lightCount); i++) {
            float lightType = lights.lightPositions[i].w;
            if (lightType >= 2.5)
                continue;
            vec3 lightPos = lights.lightPositions[i].xyz;
            vec3 lightDir = lights.lightDirectionsAndCutoff[i].xyz;
            vec3 L;
            if (lightType >= 0.5 && lightType < 1.5)
                L = normalize(-lightDir);
            else
                L = normalize(lightPos - fragPos);
            float s = GetShadowFactor(i, lightType, fragPos, N, L,
                                      receiveShadow);
            minShadow = min(minShadow, s);
        }
        outColor = vec4(vec3(minShadow), 0.0);
        return;
    }

    if (reverseZ ? (depth <= 1e-6) : (depth >= 0.999999)) {
        discard;
    }

    vec4 albedoSample = texture(inAlbedo, inUV);
    vec4 normalSample = texture(inNormal, inUV);
    vec4 pbrSample = texture(inPbr, inUV);

    uint flags = UnpackFlags(normalSample.a);
    if (flags == kFlagUnlit) {
        discard;
    }

    vec3 fragPos = ReconstructWorldPos(inUV, depth);
    vec3 albedo = srgbToLinear(albedoSample.rgb);
    float roughness = max(pbrSample.r, 0.045);
    float metalness = pbrSample.g;
    float ao = min(pbrSample.b, ssao);
    float clearcoat = clamp(pbrSample.a, 0.0, 1.0);

    float coatRoughness = 0.03;
    if (clearcoat > 1e-3) {
        uint matId = uint(clamp(round(albedoSample.a * 255.0), 0.0, 255.0));
        coatRoughness = max(materials[matId].clearcoatRoughness, 0.045);
    }

    vec3 N = normalize(normalSample.rgb * 2.0 - 1.0);
    vec3 V = normalize(lights.viewPosition.xyz - fragPos);
    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    float NdotV = max(dot(N, V), 0.0);
    vec3 Fc = fresnelSchlick(NdotV, vec3(0.04));
    float coatSpecAtten =
        (clearcoat < 1e-3)
            ? 1.0
            : (1.0 - clearcoat * max(Fc.r, max(Fc.g, Fc.b)));

    bool receiveShadow = (flags == kFlagReceiveShadow);
    // Material AO + SSAO attenuate ambient/IBL only (not direct lights).
    vec3 totalLighting =
        calculateIBL(N, V, albedo, roughness, metalness, F0, coatSpecAtten) *
        ao;
    totalLighting += calculateClearcoatIBL(N, V, coatRoughness, clearcoat) * ao;

    for (int i = 0; i < int(lights.lightCount); i++) {
        float lightType = lights.lightPositions[i].w;
        if (lightType >= 2.5) {
            totalLighting += calculateAreaLight(
                lights.lightPositions[i].xyz,
                lights.lightColorsAndRadius[i].rgb,
                lights.lightDirectionsAndCutoff[i].xyz,
                lights.lightAreaTangents[i].xyz,
                lights.lightOuterCutoffAndIntensity[i].x,
                lights.lightOuterCutoffAndIntensity[i].z,
                lights.lightOuterCutoffAndIntensity[i].y,
                fragPos, N, V, albedo, roughness, metalness, F0,
                coatSpecAtten);
            totalLighting += calculateClearcoatLight(
                lights.lightPositions[i].xyz, lightType,
                lights.lightColorsAndRadius[i].rgb,
                lights.lightColorsAndRadius[i].w,
                lights.lightDirectionsAndCutoff[i].xyz,
                lights.lightDirectionsAndCutoff[i].w,
                lights.lightOuterCutoffAndIntensity[i].x,
                lights.lightOuterCutoffAndIntensity[i].y, fragPos, N, V,
                coatRoughness, clearcoat);
            continue;
        }

        totalLighting += calculateLight(
            i,
            lights.lightPositions[i].xyz,
            lightType,
            lights.lightColorsAndRadius[i].rgb,
            lights.lightColorsAndRadius[i].w,
            lights.lightDirectionsAndCutoff[i].xyz,
            lights.lightDirectionsAndCutoff[i].w,
            lights.lightOuterCutoffAndIntensity[i].x,
            lights.lightOuterCutoffAndIntensity[i].y,
            fragPos, N, V, albedo, roughness, metalness, F0, receiveShadow,
            coatSpecAtten);
        totalLighting += calculateClearcoatLight(
            lights.lightPositions[i].xyz, lightType,
            lights.lightColorsAndRadius[i].rgb,
            lights.lightColorsAndRadius[i].w,
            lights.lightDirectionsAndCutoff[i].xyz,
            lights.lightDirectionsAndCutoff[i].w,
            lights.lightOuterCutoffAndIntensity[i].x,
            lights.lightOuterCutoffAndIntensity[i].y, fragPos, N, V,
            coatRoughness, clearcoat);
    }

    // Additive contribution; emissive already in Scene Color.
    outColor = vec4(totalLighting * lights.exposure, 0.0);
}
