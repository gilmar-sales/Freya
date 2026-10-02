#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in mat3 inTBN;
layout (location = 5) in vec3 inColor;
layout (location = 6) in vec2 inVelocity;
layout (location = 7) flat in uint inMaterialId;

layout (location = 0) out vec4 outAlbedo;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec2 outPbr;
layout (location = 3) out vec4 outSceneColor;
layout (location = 4) out vec2 outVelocity;

layout (set = 1, binding = 0) uniform sampler2D uTextures[];

#include "Include/material_gpu.inc"
#include "Include/pbr_sample.inc"
#include "Include/gbuffer.inc"

layout (std430, set = 1, binding = 1) readonly buffer MaterialBuffer {
    MaterialGPU materials[];
};

void main() {
    MaterialGPU mat = materials[inMaterialId];

    if (mat.alphaMode == 2u)
        discard;

    vec4 albedoSample = SampleBindless(mat.albedoIndex, inTexCoord);
    float alpha = albedoSample.a * mat.albedoFactor.a;
    if ((mat.alphaMode == 1u || mat.alphaCutoff > 0.0) &&
        alpha < mat.alphaCutoff)
        discard;

    // Geometric normals only — tangent maps create dense X/Y outline
    // hatch when the camera is close (cel edges sample G-buffer N).
    vec3 worldNormal = normalize(inTBN[2]);
    if (!gl_FrontFacing)
        worldNormal = -worldNormal;

    vec3 albedoLin =
        srgbToLinear(albedoSample.rgb) * inColor *
        mat.albedoFactor.rgb;

    outAlbedo = vec4(albedoLin, float(inMaterialId & 255u) / 255.0);

    uint flags = kGBufferFlagNone;
    if ((mat.flags & kMaterialFlagUnlit) != 0u)
        flags = kGBufferFlagUnlit;
    else if ((mat.flags & kMaterialFlagReceiveShadow) != 0u)
        flags = kGBufferFlagReceiveShadow;

    float roughness;
    float metalness;
    float ao;
    SamplePbrMaps(mat, inTexCoord, roughness, metalness, ao);
    // Matte response; the cel variant marks lighting_cell (not clearcoat).
    roughness = max(roughness, 0.85);
    metalness = 0.0;
    outNormal =
        GBufferPackNormal(worldNormal, roughness, kGBufferVariantCel, flags);
    outPbr = GBufferPackPbr(metalness, ao);

    outSceneColor =
        vec4(SampleEmissive(mat, inTexCoord) * kEmissiveIntensity, 0.0);
    outVelocity = inVelocity;
}
