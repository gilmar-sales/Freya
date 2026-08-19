#ifndef PBR_SAMPLE_INC
#define PBR_SAMPLE_INC

#include "pbr_common.inc"
#include "material_gpu.inc"

void SamplePbrMaps(MaterialGPU mat, vec2 uv, out float roughness,
                   out float metalness, out float ao) {
    vec4 mr =
        texture(uTextures[nonuniformEXT(mat.roughnessIndex)], uv);
    if ((mat.flags & kMaterialFlagPackedMR) != 0u) {
        roughness = max(mr.g * mat.roughMetal.x, kMinRoughness);
        metalness = mr.b * mat.roughMetal.y;
    } else {
        roughness = max(mr.r * mat.roughMetal.x, kMinRoughness);
        metalness =
            texture(uTextures[nonuniformEXT(mat.metalnessIndex)], uv).r *
            mat.roughMetal.y;
    }
    float occ =
        texture(uTextures[nonuniformEXT(mat.occlusionIndex)], uv).r;
    ao = clamp(occ * mat.emissiveFactor.w, 0.0, 1.0);
}

#endif
