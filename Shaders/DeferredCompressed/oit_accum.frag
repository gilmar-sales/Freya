#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in vec3 inColor;
layout (location = 3) flat in uint inMaterialId;
layout (location = 4) in float inViewZ;
layout (location = 5) in vec3 inWorldNormal;

layout (location = 0) out vec4 outAccum;
layout (location = 1) out float outReveal;

layout (set = 0, binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
    vec4 _pad0;
    vec4 _pad1;
    vec4 _pad2;
    mat4 invViewProjection;
    mat4 prevViewProjection;
    mat4 unjitteredProjection;
} pub;

layout (set = 1, binding = 0) uniform sampler2D uTextures[];

#include "Include/material_gpu.inc"

layout (std430, set = 1, binding = 1) readonly buffer MaterialBuffer {
    MaterialGPU materials[];
};

layout (set = 2, binding = 0) uniform LightBuffer {
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

const float kEmissiveIntensity = 1.25;

#define PBR_DIFFUSE_SCALE 0.15
#include "Include/pbr_shade.inc"

float wboitWeight(float z, float a) {
    return clamp(a * max(1e-2, 3e3 * pow(1.0 - z / 200.0, 3.0)), 1e-2, 3e3);
}

void main() {
    MaterialGPU mat = materials[inMaterialId];

    vec4 albedoSample =
        texture(uTextures[nonuniformEXT(mat.albedoIndex)], inTexCoord);
    float baseAlpha = clamp(albedoSample.a * mat.albedoFactor.a, 0.0, 1.0);
    if (baseAlpha < 1e-4)
        discard;

    vec3 albedoLin =
        srgbToLinear(albedoSample.rgb) * inColor * mat.albedoFactor.rgb;
    vec3 emissiveLin =
        srgbToLinear(
            texture(uTextures[nonuniformEXT(mat.emissiveIndex)], inTexCoord)
                .rgb) *
        mat.emissiveFactor.rgb * kEmissiveIntensity;

    float roughness = max(
        texture(uTextures[nonuniformEXT(mat.roughnessIndex)], inTexCoord).r *
            mat.roughMetal.x,
        kMinRoughness);
    float metalness = clamp(
        texture(uTextures[nonuniformEXT(mat.metalnessIndex)], inTexCoord).r *
            mat.roughMetal.y,
        0.0, 1.0);
    float clearcoat = clamp(mat.clearcoat, 0.0, 1.0);
    float coatRoughness = max(mat.clearcoatRoughness, kMinRoughness);

    vec3 N = normalize(inWorldNormal);
    vec3 V = normalize(lights.viewPosition.xyz - inWorldPos);
    if (dot(N, V) < 0.0)
        N = -N;

    vec3 F0 = mix(kDielectricF0, albedoLin, metalness);
    float NdotV = max(dot(N, V), 0.0);
    vec3 Fr = fresnelSchlick(NdotV, F0);
    float coatSpecAtten =
        (clearcoat < 1e-3)
            ? 1.0
            : (1.0 - clearcoat * max(Fr.r, max(Fr.g, Fr.b)));

    vec3 ambient = pub.ambientLight.rgb * pub.ambientLight.a * albedoLin * 0.25;
    vec3 lit = ambient;
    uint count = min(lights.lightCount, 16u);
    for (uint i = 0u; i < count; ++i) {
        lit += calculateLight(
            int(i),
            lights.lightPositions[i].xyz, lights.lightPositions[i].w,
            lights.lightColorsAndRadius[i].rgb,
            lights.lightColorsAndRadius[i].w,
            lights.lightDirectionsAndCutoff[i].xyz,
            lights.lightDirectionsAndCutoff[i].w,
            lights.lightOuterCutoffAndIntensity[i].x,
            lights.lightOuterCutoffAndIntensity[i].y, inWorldPos, N, V,
            albedoLin, roughness, metalness, F0, false, coatSpecAtten);
        lit += calculateClearcoatLight(
            lights.lightPositions[i].xyz, lights.lightPositions[i].w,
            lights.lightColorsAndRadius[i].rgb,
            lights.lightColorsAndRadius[i].w,
            lights.lightDirectionsAndCutoff[i].xyz,
            lights.lightDirectionsAndCutoff[i].w,
            lights.lightOuterCutoffAndIntensity[i].x,
            lights.lightOuterCutoffAndIntensity[i].y, inWorldPos, N, V,
            coatRoughness, clearcoat);
    }

    vec3 C = lit * (1.0 - Fr) + lit * Fr * 2.5 + emissiveLin;

    float fresnelCoverage = max(Fr.r, max(Fr.g, Fr.b));
    float alpha = clamp(max(baseAlpha, fresnelCoverage * 0.85), 0.0, 1.0);

    float w = wboitWeight(max(inViewZ, 1e-3), alpha);
    outAccum = vec4(C * alpha * w, alpha * w);
    outReveal = alpha;
}
