#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

// Triplanar albedo/normal sampling in world space (uses material albedo map).

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in mat3 inTBN;
layout (location = 5) in vec3 inColor;
layout (location = 6) in vec2 inVelocity;
layout (location = 7) flat in uint inMaterialId;

layout (location = 0) out vec4 outAlbedo;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outPbr;
layout (location = 3) out vec4 outSceneColor;
layout (location = 4) out vec2 outVelocity;

layout (set = 1, binding = 0) uniform sampler2D uTextures[];

#include "Include/material_gpu.inc"
#include "Include/pbr_sample.inc"

layout (std430, set = 1, binding = 1) readonly buffer MaterialBuffer {
    MaterialGPU materials[];
};

const float kEmissiveIntensity = 2.0;
const float kTriScale = 0.35;
const uint kFlagReceiveShadow = 1u;
const uint kFlagUnlit = 3u;

vec3 TriplanarWeights(vec3 n) {
    vec3 w = abs(n);
    w = max(w, vec3(0.0));
    w = pow(w, vec3(4.0));
    return w / max(w.x + w.y + w.z, 1e-4);
}

vec4 TriplanarSample(uint texIndex, vec3 worldPos, vec3 weights) {
    vec4 x = texture(uTextures[nonuniformEXT(texIndex)],
                     worldPos.zy * kTriScale);
    vec4 y = texture(uTextures[nonuniformEXT(texIndex)],
                     worldPos.xz * kTriScale);
    vec4 z = texture(uTextures[nonuniformEXT(texIndex)],
                     worldPos.xy * kTriScale);
    return x * weights.x + y * weights.y + z * weights.z;
}

void main() {
    MaterialGPU mat = materials[inMaterialId];

    if (mat.alphaMode == 2u)
        discard;

    vec3 worldNormal = normalize(inTBN[2]);
    if (!gl_FrontFacing)
        worldNormal = -worldNormal;
    vec3 weights = TriplanarWeights(worldNormal);

    vec4 albedoSample = TriplanarSample(mat.albedoIndex, inPosition, weights);
    float alpha = albedoSample.a * mat.albedoFactor.a;
    if ((mat.alphaMode == 1u || mat.alphaCutoff > 0.0) &&
        alpha < mat.alphaCutoff)
        discard;

    vec3 albedoLin =
        srgbToLinear(albedoSample.rgb) * inColor * mat.albedoFactor.rgb;

    // Blend geometric normal with optional normal map (also triplanar).
    vec3 nSample = TriplanarSample(mat.normalIndex, inPosition, weights).rgb;
    if (!all(greaterThan(nSample, vec3(0.99)))) {
        // Cheap: lean geometric N toward map without full TBN rebuild.
        vec3 mapped = normalize(nSample * 2.0 - 1.0);
        worldNormal = normalize(mix(worldNormal, worldNormal + mapped * 0.35, 0.5));
    }

    outAlbedo = vec4(linearToSrgb(albedoLin),
                     float(inMaterialId & 255u) / 255.0);

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
    outSceneColor = vec4(emissiveLin * kEmissiveIntensity, 0.0);
    outVelocity = inVelocity;
}
