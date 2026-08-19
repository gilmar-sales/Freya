#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in mat3 inTBN;
layout (location = 5) in vec3 inColor;
layout (location = 6) in vec2 inVelocity;
layout (location = 7) flat in uint inMaterialId;

layout (location = 0) out vec4 outAlbedo;     // RGB albedo (gamma), A matID
layout (location = 1) out vec4 outNormal;     // RGB packed normal, A 2-bit flags
layout (location = 2) out vec4 outPbr;        // R rough, G metal, B AO|coatR, A coat
layout (location = 3) out vec4 outSceneColor; // HDR emissive
layout (location = 4) out vec2 outVelocity;   // UV-space motion

layout (set = 1, binding = 0) uniform sampler2D uTextures[];

#include "Include/material_gpu.inc"
#include "Include/pbr_sample.inc"

layout (std430, set = 1, binding = 1) readonly buffer MaterialBuffer {
    MaterialGPU materials[];
};

const float kEmissiveIntensity = 2.0;

const uint kFlagReceiveShadow = 1u;
const uint kFlagIgnoreDecals  = 2u;
const uint kFlagUnlit         = 3u;

void main() {
    MaterialGPU mat = materials[inMaterialId];

    if (mat.alphaMode == 2u)
        discard;

    vec4 albedoSample =
        texture(uTextures[nonuniformEXT(mat.albedoIndex)], inTexCoord);
    float alpha = albedoSample.a * mat.albedoFactor.a;
    if ((mat.alphaMode == 1u || mat.alphaCutoff > 0.0) &&
        alpha < mat.alphaCutoff)
        discard;

    vec3 sampled =
        texture(uTextures[nonuniformEXT(mat.normalIndex)], inTexCoord).rgb;
    vec3 worldNormal;
    if (all(greaterThan(sampled, vec3(0.99)))) {
        worldNormal = normalize(inTBN[2]);
    } else {
        vec3 tangentNormal = sampled * 2.0 - 1.0;
        worldNormal = normalize(inTBN * tangentNormal);
    }
    if (!gl_FrontFacing)
        worldNormal = -worldNormal;

    vec3 albedoLin =
        srgbToLinear(albedoSample.rgb) * inColor *
        mat.albedoFactor.rgb;

    outAlbedo = vec4(linearToSrgb(albedoLin),
                     mod(mat.materialId, 256.0) / 255.0);

    uint flags = 0u;
    if ((mat.flags & kMaterialFlagUnlit) != 0u)
        flags = kFlagUnlit;
    else if ((mat.flags & kMaterialFlagReceiveShadow) != 0u)
        flags = kFlagReceiveShadow;
    outNormal = vec4(worldNormal * 0.5 + 0.5, float(flags) / 3.0);

    float roughness;
    float metalness;
    float ao;
    SamplePbrMaps(mat, inTexCoord, roughness, metalness, ao);
    float clearcoat = clamp(mat.clearcoat, 0.0, 1.0);
    float pbrB = (clearcoat > 1e-3)
                     ? max(mat.clearcoatRoughness, kMinRoughness)
                     : ao;
    outPbr = vec4(roughness, metalness, pbrB, clearcoat);

    vec3 emissiveLin =
        srgbToLinear(
            texture(uTextures[nonuniformEXT(mat.emissiveIndex)], inTexCoord)
                .rgb) *
        mat.emissiveFactor.rgb;
    outSceneColor = vec4(emissiveLin * kEmissiveIntensity, 1.0);
    outVelocity = inVelocity;
}
