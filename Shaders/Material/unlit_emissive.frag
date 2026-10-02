#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

// Unlit / emissive G-buffer: albedo goes straight to HDR scene color.

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

    vec3 worldNormal = normalize(inTBN[2]);
    if (!gl_FrontFacing)
        worldNormal = -worldNormal;

    vec3 albedoLin =
        srgbToLinear(albedoSample.rgb) * inColor * mat.albedoFactor.rgb;
    vec3 emissiveLin = SampleEmissive(mat, inTexCoord);

    outAlbedo = vec4(albedoLin, float(inMaterialId & 255u) / 255.0);
    outNormal = GBufferPackNormal(worldNormal, 1.0, kGBufferVariantNone,
                                  kGBufferFlagUnlit);
    outPbr = GBufferPackPbr(0.0, 1.0);
    // Unlit path in lighting uses emissive/scene; fold albedo into HDR.
    outSceneColor =
        vec4((albedoLin + emissiveLin) * kEmissiveIntensity, 0.0);
    outVelocity = inVelocity;
}
