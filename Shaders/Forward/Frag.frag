#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

layout(binding = 1) uniform LightBuffer {
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

layout(binding = 2) uniform sampler2D irradianceMap;
layout(binding = 3) uniform sampler2D prefilterMap;
layout(binding = 4) uniform sampler2D brdfLUT;
layout(binding = 5) uniform sampler2D ltcMatrixMap;
layout(binding = 6) uniform sampler2D ltcAmplMap;

layout(binding = 7) uniform ShadowBuffer {
    mat4 cascadeViewProj[4];
    vec4 cascadeSplits;
    vec4 params; // x=bias y=normalBias z=cascadeCount w=softScale
    mat4 spotViewProj[4];
    vec4 spotLightIndex;
    vec4 pointLightPosFar[2];
    vec4 pointLightIndex;
    vec4 reverseZ; // x=1 when Reverse-Z point depth encoding is active
    vec4 pcss; // x=lightSize y=maxSoftness z=minVisibility w=tapCount
} shadows;

layout(binding = 8) uniform sampler2DArrayShadow cascadeShadowMap;
layout(binding = 9) uniform sampler2DArrayShadow spotShadowMap;
layout(binding = 10) uniform samplerCubeArrayShadow pointShadowMap;
layout(binding = 11) uniform sampler2DArray cascadeShadowDepth;
layout(binding = 12) uniform sampler2DArray spotShadowDepth;
layout(binding = 13) uniform sampler2D directionalShadowMask;

layout(set = 1, binding = 0) uniform sampler2D albedoSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;
layout(set = 1, binding = 2) uniform sampler2D roughnessSampler;
layout(set = 1, binding = 3) uniform sampler2D emissiveSampler;
layout(set = 1, binding = 4) uniform sampler2D metalnessSampler;

layout(set = 1, binding = 5) uniform MaterialFactors {
    vec4 albedoFactor;
    vec4 emissiveFactor; // xyz used, w unused
    vec2 roughMetal;     // x = roughnessFactor, y = metalnessFactor
} materialFactors;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosition;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in mat3 TBN;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const float LUT_SIZE = 64.0;
const float LUT_SCALE = (LUT_SIZE - 1.0) / LUT_SIZE;
const float LUT_BIAS = 0.5 / LUT_SIZE;

vec3 getNormalFromMap() {
    vec3 sampled = texture(normalSampler, fragTexCoord).rgb;
    // Pure white fallback means "no normal map" — use geometric normal.
    if (all(greaterThan(sampled, vec3(0.99))))
        return normalize(TBN[2]);
    vec3 tangentNormal = sampled * 2.0 - 1.0;
    return normalize(TBN * tangentNormal);
}

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

// Identical soft disk for every light type (point-style angular / TBN).
const float kSoftDisk = 0.008;

int SoftShadowTapCount() {
    return clamp(int(shadows.pcss.w + 0.5), 1, 16);
}

void ShadowTangentBasis(vec3 axis, out vec3 T, out vec3 B) {
    vec3 up = abs(axis.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    T = normalize(cross(up, axis));
    B = cross(axis, T);
}

// Point technique on a 2D map: same angular Poisson as cube shadows, with a
// fixed center depthRef (varying depth per tap is what made dir/spot grainy).
float SoftShadow2DPointStyle(sampler2DArrayShadow map, mat4 lightVP,
                             vec3 lightPos, vec3 biased, float layer,
                             float depthBiasAmount) {
    vec3 dir = biased - lightPos;
    float dist = length(dir);
    if (dist < 1e-5)
        return 1.0;

    vec3 Ndir = dir / dist;
    vec3 T, B;
    ShadowTangentBasis(Ndir, T, B);
    bool reverseZ = shadows.reverseZ.x > 0.5;

    vec4 centerClip = lightVP * vec4(biased, 1.0);
    vec3 centerNdc = centerClip.xyz / centerClip.w;
    if (abs(centerNdc.x) > 1.0 || abs(centerNdc.y) > 1.0)
        return 1.0;
    float depthRef = reverseZ ? (centerNdc.z + depthBiasAmount)
                              : (centerNdc.z - depthBiasAmount);

    int tapCount = SoftShadowTapCount();
    float result = 0.0;
    for (int i = 0; i < 16; ++i) {
        if (i >= tapCount)
            break;
        vec2 o = kPoissonDisk[i] * kSoftDisk;
        vec3 sampleDir = normalize(Ndir + T * o.x + B * o.y);
        vec3 samplePos = lightPos + sampleDir * dist;
        vec4 clip = lightVP * vec4(samplePos, 1.0);
        vec3 ndc = clip.xyz / clip.w;
        if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0) {
            result += 1.0;
            continue;
        }
        vec2 uv = ndc.xy * 0.5 + 0.5;
        result += texture(map, vec4(uv, layer, depthRef));
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

    // Match point bias scaling: treat "far" as softDist (virtual light).
    float softDist = 20.0;
    float nDotL = max(dot(N, normalize(L)), 0.0);
    float slope = sqrt(max(1.0 - nDotL * nDotL, 0.0));
    float normalScale = shadows.params.y * (0.35 + 0.65 * slope);
    float lightPush = shadows.params.x * softDist * (0.5 + slope);
    vec3 lightDir = normalize(L);
    vec3 biased = worldPos + N * normalScale + lightDir * lightPush;
    float depthBiasAmount = shadows.params.x * (1.0 + 1.5 * slope);
    vec3 lightPos = biased - lightDir * softDist;

    float shadow = SoftShadow2DPointStyle(
        cascadeShadowMap, shadows.cascadeViewProj[cascade], lightPos, biased,
        float(cascade), depthBiasAmount);

    if (cascade < cascadeCount - 1) {
        float splitNear =
            (cascade == 0) ? 0.0 : shadows.cascadeSplits[cascade - 1];
        float splitFar = shadows.cascadeSplits[cascade];
        float blendStart = mix(splitNear, splitFar, 0.9);
        if (viewDepth > blendStart) {
            float next = SoftShadow2DPointStyle(
                cascadeShadowMap, shadows.cascadeViewProj[cascade + 1],
                lightPos, biased, float(cascade + 1), depthBiasAmount);
            float t = smoothstep(blendStart, splitFar, viewDepth);
            shadow = mix(shadow, next, t);
        }
    }
    return shadow;
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
    float radius = lights.lightColorsAndRadius[lightIndex].w;
    float distHint = max(length(lightPos - worldPos), 1.0);
    float nDotL = max(dot(N, normalize(L)), 0.0);
    float slope = sqrt(max(1.0 - nDotL * nDotL, 0.0));
    float normalScale = shadows.params.y * (0.35 + 0.65 * slope);
    // Same scale as point (point uses radius/far).
    float lightPush =
        shadows.params.x * min(distHint, radius) * (0.5 + slope);
    vec3 biased = worldPos + N * normalScale + normalize(L) * lightPush;
    float depthBiasAmount = shadows.params.x * (1.0 + 1.5 * slope);

    return SoftShadow2DPointStyle(spotShadowMap, shadows.spotViewProj[slot],
                                  lightPos, biased, float(slot),
                                  depthBiasAmount);
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
    float normalScale = shadows.params.y * (0.35 + 0.65 * slope);
    float lightPush = shadows.params.x * far * (0.5 + slope);
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
    int tapCount = SoftShadowTapCount();
    float result = 0.0;
    for (int i = 0; i < 16; ++i) {
        if (i >= tapCount)
            break;
        vec2 o = kPoissonDisk[i] * kSoftDisk;
        vec3 sampleDir = normalize(Ndir + T * o.x + B * o.y);
        result += texture(pointShadowMap, vec4(sampleDir, float(slot)),
                          depthRef);
    }
    return result / float(tapCount);
}

float GetShadowFactor(int i, float lightType, vec3 worldPos, vec3 N,
                      vec3 L) {
    if (lights.lightOuterCutoffAndIntensity[i].w < 0.5)
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
                        vec3 F0) {
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
    specular *= spec;
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
                    float roughness, float metalness, vec3 F0) {
    vec3 L;
    float attenuation;
    resolveLight(lightPos, lightType, radius, lightDir, innerCutoff,
                 outerCutoff, worldPos, L, attenuation);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0 || attenuation <= 0.0) {
        return vec3(0.0);
    }

    float shadowFactor =
        GetShadowFactor(lightIndex, lightType, worldPos, N, L);

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
    return (kD * albedo / PI + specular) * radiance * NdotL * shadowFactor;
}

vec3 calculateIBL(vec3 N, vec3 V, vec3 albedo, float roughness, float metalness,
                  vec3 F0) {
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
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    return (kD * diffuse + specular) * lights.iblIntensity;
}

vec3 srgbToLinear(vec3 c) {
    return pow(c, vec3(2.2));
}

vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 albedo = srgbToLinear(texture(albedoSampler, fragTexCoord).rgb) *
                  fragColor * materialFactors.albedoFactor.rgb;
    vec3 normal = getNormalFromMap();
    float roughness =
        max(texture(roughnessSampler, fragTexCoord).r *
                materialFactors.roughMetal.x,
            0.045);
    float metalness = texture(metalnessSampler, fragTexCoord).r *
                      materialFactors.roughMetal.y;
    vec3 emissive = srgbToLinear(texture(emissiveSampler, fragTexCoord).rgb) *
                    materialFactors.emissiveFactor.rgb;

    vec3 N = normalize(normal);
    vec3 V = normalize(lights.viewPosition.xyz - fragPosition);
    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    vec3 totalLighting = calculateIBL(N, V, albedo, roughness, metalness, F0);

    totalLighting +=
        albedo * pub.ambientLight.rgb * pub.ambientLight.w * (1.0 - metalness);

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
                fragPosition, N, V, albedo, roughness, metalness, F0);
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
            fragPosition, N, V, albedo, roughness, metalness, F0);
    }

    vec3 color = totalLighting * lights.exposure + emissive;
    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
