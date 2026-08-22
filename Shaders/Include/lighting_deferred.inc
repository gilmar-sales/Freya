// Shared deferred lighting body. Define FREYA_CEL_LIGHTING before include
// for gbuffer_cell cel banding (PBR.a > 0.5 marks cel, not clearcoat).

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
    vec4 lightPositions[FREYA_MAX_LIGHTS];
    vec4 lightColorsAndRadius[FREYA_MAX_LIGHTS];
    vec4 lightDirectionsAndCutoff[FREYA_MAX_LIGHTS];
    vec4 lightOuterCutoffAndIntensity[FREYA_MAX_LIGHTS];
    vec4 lightAreaTangents[FREYA_MAX_LIGHTS];
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

layout(push_constant) uniform LightingPush {
    uint debugMode; // 0 = lit, 1 = SSAO, 2 = shadow factor
    uint _pad0;
    uint _pad1;
    uint _pad2;
} push;

layout(location = 0) out vec4 outColor;

const uint kFlagReceiveShadow = 1u;
const uint kFlagUnlit = 3u;

#define PBR_WITH_SHADOWS
#include "Include/shadows.inc"
#include "Include/ltc.inc"
#include "Include/pbr_ibl.inc"
#include "Include/pbr_shade.inc"

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
    float roughness = max(pbrSample.r, kMinRoughness);
    float metalness = pbrSample.g;
#ifdef FREYA_CEL_LIGHTING
    // gbuffer_cell: PBR.a = 1 marks cel (not clearcoat).
    bool applyCel = pbrSample.a > 0.5;
    float clearcoat = applyCel ? 0.0 : clamp(pbrSample.a, 0.0, 1.0);
#else
    float clearcoat = clamp(pbrSample.a, 0.0, 1.0);
#endif
    // PBR.b is coat roughness when coated, else material AO.
    float coatRoughness = kMinRoughness;
    float ao;
    if (clearcoat > 1e-3) {
        coatRoughness = max(pbrSample.b, kMinRoughness);
        ao = ssao;
    } else {
        ao = min(pbrSample.b, ssao);
    }

    vec3 N = normalize(normalSample.rgb * 2.0 - 1.0);
    vec3 V = normalize(lights.viewPosition.xyz - fragPos);
    vec3 F0 = mix(kDielectricF0, albedo, metalness);

    float NdotV = max(dot(N, V), 0.0);
    vec3 Fc = fresnelSchlick(NdotV, kDielectricF0);
    float coatSpecAtten =
        (clearcoat < 1e-3)
            ? 1.0
            : (1.0 - clearcoat * max(Fc.r, max(Fc.g, Fc.b)));

    bool receiveShadow = (flags == kFlagReceiveShadow);
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

#ifdef FREYA_CEL_LIGHTING
    if (!applyCel) {
        outColor = vec4(totalLighting * lights.exposure, 0.0);
        return;
    }

    const float kBands = 4.0;
    const float kShadowLift = 0.22;
    vec3 lumaW = vec3(0.2126, 0.7152, 0.0722);
    float luma = max(dot(totalLighting, lumaW), 0.0);
    float albedoLuma = max(dot(albedo, lumaW), 1e-4);
    float light = luma / albedoLuma;
    float soft = light * (1.0 - kShadowLift) + kShadowLift;
    float cel = floor(soft * kBands + 1e-4) / kBands;
    outColor = vec4(albedo * cel * lights.exposure, 0.0);
#else
    outColor = vec4(totalLighting * lights.exposure, 0.0);
#endif
}
