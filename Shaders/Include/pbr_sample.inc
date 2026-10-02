#ifndef PBR_SAMPLE_INC
#define PBR_SAMPLE_INC

#include "material_gpu.inc"
#include "pbr_common.inc"

vec4 SampleBindless(uint index, vec2 uv)
{
    if (index == kBindlessWhiteTexture)
        return vec4(1.0);

    if (index == kBindlessBlackTexture)
        return vec4(0.0, 0.0, 0.0, 1.0);

    return texture(uTextures[nonuniformEXT(index)], uv);
}

void SamplePbrMaps(MaterialGPU mat, vec2 uv, out float roughness,
                   out float metalness, out float ao)
{
    vec4 mr = SampleBindless(mat.roughnessIndex, uv);
    if ((mat.flags & kMaterialFlagPackedMR) != 0u)
    {
        roughness = max(mr.g * mat.roughMetal.x, kMinRoughness);
        metalness = mr.b * mat.roughMetal.y;
    }
    else
    {
        roughness = max(mr.r * mat.roughMetal.x, kMinRoughness);
        metalness = SampleBindless(mat.metalnessIndex, uv).r * mat.roughMetal.y;
    }
    float occ = SampleBindless(mat.occlusionIndex, uv).r;
    ao        = clamp(occ * mat.emissiveFactor.w, 0.0, 1.0);
}

// Emissive radiance (linear, unscaled by kEmissiveIntensity). Skips the
// bindless fetch for the common case of a material without emissive.
vec3 SampleEmissive(MaterialGPU mat, vec2 uv)
{
    if (mat.emissiveIndex == kBindlessBlackTexture ||
        dot(mat.emissiveFactor.rgb, vec3(1.0)) <= 0.0)
        return vec3(0.0);
    return srgbToLinear(
               texture(uTextures[nonuniformEXT(mat.emissiveIndex)], uv).rgb) *
           mat.emissiveFactor.rgb;
}

#endif
