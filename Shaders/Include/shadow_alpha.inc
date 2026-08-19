#ifndef SHADOW_ALPHA_INC
#define SHADOW_ALPHA_INC

#include "material_gpu.inc"

layout(set = 1, binding = 0) uniform sampler2D uTextures[];

layout(std430, set = 1, binding = 1) readonly buffer MaterialBuffer {
    MaterialGPU materials[];
};

void discardMaskedShadow(vec2 uv, uint materialId)
{
    MaterialGPU mat = materials[materialId];
    if (mat.alphaMode != 1u && mat.alphaCutoff <= 0.0)
        return;
    float alpha =
        texture(uTextures[nonuniformEXT(mat.albedoIndex)], uv).a *
        mat.albedoFactor.a;
    if (alpha < mat.alphaCutoff)
        discard;
}

#endif
