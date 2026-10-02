// Shared deferred lighting body. Define FREYA_CEL_LIGHTING before include
// for gbuffer_cell cel banding (PBR.a > 0.5 marks cel, not clearcoat).

layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D inDepth;
layout(set = 0, binding = 1) uniform sampler2D inAlbedo;
layout(set = 0, binding = 2) uniform sampler2D inNormal;
layout(set = 0, binding = 3) uniform sampler2D inPbr;

layout(set = 0, binding = 4) uniform CameraBuffer
{
    mat4 view;
    mat4 projection;
    vec4 ambientLight;
    // Match C++ alignas(64) between ambient and invViewProjection.
    vec4 _pad0;
    vec4 _pad1;
    vec4 _pad2;
    mat4 invViewProjection;
}
camera;

layout(set = 0, binding = 5) uniform LightBuffer
{
    vec4  lightPositions[FREYA_MAX_LIGHTS];
    vec4  lightColorsAndRadius[FREYA_MAX_LIGHTS];
    vec4  lightDirectionsAndCutoff[FREYA_MAX_LIGHTS];
    vec4  lightOuterCutoffAndIntensity[FREYA_MAX_LIGHTS];
    vec4  lightAreaTangents[FREYA_MAX_LIGHTS];
    vec4  viewPosition;
    vec4  cameraForward;
    uint  lightCount;
    float iblIntensity;
    float exposure;
}
lights;

layout(set = 0, binding = 6) uniform sampler2D irradianceMap;
layout(set = 0, binding = 7) uniform sampler2D prefilterMap;
layout(set = 0, binding = 8) uniform sampler2D brdfLUT;
layout(set = 0, binding = 9) uniform sampler2D ltcMatrixMap;
layout(set = 0, binding = 10) uniform sampler2D ltcAmplMap;

layout(set = 0, binding = 11) uniform ShadowBuffer
{
    mat4 cascadeViewProj[4];
    vec4 cascadeSplits;
    vec4 params; // x=bias y=normalBias(texels) z=cascadeCount w=softScale
    mat4 spotViewProj[4];
    vec4 spotLightIndex;
    vec4 pointLightPosFar[2];
    vec4 pointLightIndex;
    vec4 reverseZ; // x=Reverse-Z y=cascadeRes z=cascadeBlend w=useMask
    vec4 pcss;     // x=lightSize(world) y=maxSoft(texels) z=minVis w=tapCount
    vec4 cascadeTexelSize; // world-space texel size per cascade
}
shadows;

layout(set = 0, binding = 12) uniform sampler2DArrayShadow cascadeShadowMap;
layout(set = 0, binding = 13) uniform sampler2DArrayShadow spotShadowMap;
layout(set = 0, binding = 14) uniform samplerCubeArrayShadow pointShadowMap;
layout(set = 0, binding = 15) uniform sampler2D inSsao;
layout(set = 0, binding = 16) uniform sampler2D inVelocity;
layout(set = 0, binding = 17) uniform sampler2D directionalShadowMask;
// Order-2 SH of the irradiance map: diffuse IBL without a texture fetch.
layout(set = 0, binding = 18) uniform IrradianceShBuffer
{
    vec4 irradianceSh[9];
};

layout(push_constant) uniform LightingPush
{
    uint debugMode; // DeferredDebugView (0 = lit)
    uint _pad0;
    uint _pad1;
    uint _pad2;
}
push;

layout(location = 0) out vec4 outColor;

vec2 gLightingUV;

#define PBR_WITH_SHADOWS
#define FREYA_IBL_SH
#define FREYA_DIRECTIONAL_SHADOW_MASK
#include "Include/ltc.inc"
#include "Include/pbr_ibl.inc"
#include "Include/shadows.inc"
#include "Include/pbr_shade.inc"
#include "Include/gbuffer.inc"

vec3 ReconstructWorldPos(vec2 uv, float depth)
{
    vec4 clip  = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = camera.invViewProjection * clip;
    return world.xyz / world.w;
}

// Linearize depth for visualization (works for Reverse-Z and standard).
float VisualizeDepth(float depth, bool reverseZ)
{
    if (reverseZ)
    {
        // Near ≈ 1, far ≈ 0 — flip and compress for readability.
        float t = clamp(1.0 - depth, 0.0, 1.0);
        return pow(t, 0.35);
    }
    return pow(clamp(depth, 0.0, 1.0), 0.35);
}

void main()
{
    gLightingUV    = inUV;
    float depth    = texture(inDepth, inUV).r;
    bool  reverseZ = shadows.reverseZ.x > 0.5;
    bool  sky      = reverseZ ? (depth <= 1e-6) : (depth >= 0.999999);

    // DeferredDebugView — LDR output, composite skips ACES.
    if (push.debugMode != 0u)
    {
        if (sky && push.debugMode != 3u)
        {
            outColor = vec4(1.0, 1.0, 1.0, 0.0);
            return;
        }

        if (push.debugMode == 1u)
        { // Albedo
            outColor = vec4(linearToSrgb(texture(inAlbedo, inUV).rgb), 0.0);
            return;
        }
        if (push.debugMode == 2u)
        { // World normal
            vec3 N   = GBufferUnpackNormal(texture(inNormal, inUV));
            outColor = vec4(N * 0.5 + 0.5, 0.0);
            return;
        }
        if (push.debugMode == 3u)
        { // Depth
            float d  = sky ? 0.0 : VisualizeDepth(depth, reverseZ);
            outColor = vec4(vec3(d), 0.0);
            return;
        }
        if (push.debugMode == 4u)
        { // Roughness
            outColor = vec4(vec3(GBufferUnpackRoughness(texture(inNormal, inUV))), 0.0);
            return;
        }
        if (push.debugMode == 5u)
        { // Metalness
            outColor = vec4(vec3(GBufferUnpackMetalness(texture(inPbr, inUV).rg)), 0.0);
            return;
        }
        if (push.debugMode == 6u)
        { // Material AO (PBR.b when uncoated)
            vec4  nrm = texture(inNormal, inUV);
            float ao  = (GBufferUnpackVariant(nrm) == kGBufferVariantCoat)
                            ? 1.0
                            : GBufferUnpackAo(texture(inPbr, inUV).rg);
            outColor  = vec4(vec3(ao), 0.0);
            return;
        }
        if (push.debugMode == 7u)
        { // Material ID (albedo.a)
            outColor = vec4(vec3(texture(inAlbedo, inUV).a), 0.0);
            return;
        }
        if (push.debugMode == 8u)
        { // Velocity → R=G, G=B scaled
            vec2 vel = texture(inVelocity, inUV).rg;
            outColor = vec4(vel * 10.0 + 0.5, 0.5, 0.0);
            return;
        }
        if (push.debugMode == 9u || push.debugMode == 10u)
        { // SSAO
            float ssao = texture(inSsao, inUV).r;
            float ao   = sky ? 1.0 : ssao;
            outColor   = vec4(vec3(ao), 0.0);
            return;
        }
        if (push.debugMode == 11u)
        { // Shadows
            vec3  fragPos = ReconstructWorldPos(inUV, depth);
            vec4  nrm     = texture(inNormal, inUV);
            vec3  N       = GBufferUnpackNormal(nrm);
            uint  flags   = GBufferUnpackFlags(nrm);
            bool  receiveShadow = (flags == kGBufferFlagReceiveShadow);
            float minShadow     = 1.0;
            for (int i = 0; i < int(lights.lightCount); i++)
            {
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
                float s =
                    GetShadowFactor(i, lightType, fragPos, N, L, receiveShadow);
                minShadow = min(minShadow, s);
            }
            outColor = vec4(vec3(minShadow), 0.0);
            return;
        }
        outColor = vec4(1.0, 0.0, 1.0, 0.0);
        return;
    }

    if (sky)
    {
        discard;
    }

    float ssao = texture(inSsao, inUV).r;

    vec4 albedoSample = texture(inAlbedo, inUV);
    vec4 normalSample = texture(inNormal, inUV);
    vec2 pbrSample    = texture(inPbr, inUV).rg;

    uint flags = GBufferUnpackFlags(normalSample);
    if (flags == kGBufferFlagUnlit)
    {
        discard;
    }

    vec3 fragPos = ReconstructWorldPos(inUV, depth);
    // The sRGB G-buffer performs the RGB decode during sampling.
    vec3  albedo    = albedoSample.rgb;
    float roughness = max(GBufferUnpackRoughness(normalSample), kMinRoughness);
    float metalness = GBufferUnpackMetalness(pbrSample);
    uint  variant   = GBufferUnpackVariant(normalSample);
#ifdef FREYA_CEL_LIGHTING
    // gbuffer_cell marks cel pixels with the cel variant (not clearcoat).
    bool  applyCel  = (variant == kGBufferVariantCel);
    float clearcoat = (variant == kGBufferVariantCoat)
                          ? GBufferUnpackClearcoat(pbrSample)
                          : 0.0;
#else
    float clearcoat = (variant == kGBufferVariantCoat)
                          ? GBufferUnpackClearcoat(pbrSample)
                          : 0.0;
#endif
    // G channel is coat roughness when coated, else material AO.
    float coatRoughness = kMinRoughness;
    float ao;
    if (clearcoat > 1e-3)
    {
        coatRoughness = max(GBufferUnpackCoatRoughness(pbrSample), kMinRoughness);
        ao            = ssao;
    }
    else
    {
        ao = min(GBufferUnpackAo(pbrSample), ssao);
    }

    vec3 N  = GBufferUnpackNormal(normalSample);
    vec3 V  = normalize(lights.viewPosition.xyz - fragPos);
    vec3 F0 = mix(kDielectricF0, albedo, metalness);

    float NdotV = max(dot(N, V), 0.0);
    vec3  Fc    = fresnelSchlick(NdotV, kDielectricF0);
    float coatSpecAtten =
        (clearcoat < 1e-3) ? 1.0
                           : (1.0 - clearcoat * max(Fc.r, max(Fc.g, Fc.b)));

    bool receiveShadow = (flags == kGBufferFlagReceiveShadow);
    vec3 totalLighting =
        calculateIBL(N, V, albedo, roughness, metalness, F0, coatSpecAtten) *
        ao;
    totalLighting += calculateClearcoatIBL(N, V, coatRoughness, clearcoat) * ao;

    for (int i = 0; i < int(lights.lightCount); i++)
    {
        float lightType = lights.lightPositions[i].w;
        if (lightType >= 2.5)
        {
            totalLighting += calculateAreaLight(
                lights.lightPositions[i].xyz,
                lights.lightColorsAndRadius[i].rgb,
                lights.lightDirectionsAndCutoff[i].xyz,
                lights.lightAreaTangents[i].xyz,
                lights.lightOuterCutoffAndIntensity[i].x,
                lights.lightOuterCutoffAndIntensity[i].z,
                lights.lightOuterCutoffAndIntensity[i].y, fragPos, N, V, albedo,
                roughness, metalness, F0, coatSpecAtten);
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
            i, lights.lightPositions[i].xyz, lightType,
            lights.lightColorsAndRadius[i].rgb,
            lights.lightColorsAndRadius[i].w,
            lights.lightDirectionsAndCutoff[i].xyz,
            lights.lightDirectionsAndCutoff[i].w,
            lights.lightOuterCutoffAndIntensity[i].x,
            lights.lightOuterCutoffAndIntensity[i].y, fragPos, N, V, albedo,
            roughness, metalness, F0, receiveShadow, coatSpecAtten);
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
    if (!applyCel)
    {
        outColor = vec4(max(totalLighting * lights.exposure, 0.0), 0.0);
        return;
    }

    const float kBands      = 4.0;
    const float kShadowLift = 0.22;
    vec3        lumaW       = vec3(0.2126, 0.7152, 0.0722);
    float       luma        = max(dot(totalLighting, lumaW), 0.0);
    float       albedoLuma  = max(dot(albedo, lumaW), 1e-4);
    float       light       = luma / albedoLuma;
    float       soft        = light * (1.0 - kShadowLift) + kShadowLift;
    float       cel         = floor(soft * kBands + 1e-4) / kBands;
    outColor                = vec4(albedo * cel * lights.exposure, 0.0);
#else
    outColor = vec4(max(totalLighting * lights.exposure, 0.0), 0.0);
#endif
}
