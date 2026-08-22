#ifndef POST_MASK_INC
#define POST_MASK_INC

layout(set = 1, binding = 0) uniform sampler2D inAlbedo;
layout(std140, set = 1, binding = 1) uniform MaterialMask {
    uvec4 bits[8];
    uint count;
} mask;

bool MaterialIncluded(uint matId) {
    if (mask.count == 0u)
        return true;
    uint word = mask.bits[matId >> 7u][(matId >> 5u) & 3u];
    return (word & (1u << (matId & 31u))) != 0u;
}

uint SampleMaterialId(vec2 uv) {
    return uint(texture(inAlbedo, uv).a * 255.0 + 0.5);
}

#endif
