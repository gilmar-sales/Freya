#ifndef MATERIAL_GPU_INC
#define MATERIAL_GPU_INC

const uint kMaterialFlagPackedMR = 1u;
const uint kMaterialFlagUnlit = 2u;
const uint kMaterialFlagDoubleSided = 4u;
const uint kMaterialFlagReceiveShadow = 8u;

// Must match C++ MaterialGPU (std430, 96 bytes).
struct MaterialGPU {
    uint albedoIndex;
    uint normalIndex;
    uint roughnessIndex;
    uint emissiveIndex;
    uint metalnessIndex;
    uint alphaMode; // 0 Opaque, 1 Mask, 2 Blend
    float clearcoat;
    float clearcoatRoughness;
    vec4 albedoFactor;
    vec4 emissiveFactor; // xyz emissive, w = aoFactor
    vec2 roughMetal;
    float materialId;
    float alphaCutoff;
    uint occlusionIndex;
    uint flags;
    float transmission;
    float ior;
};

#endif
