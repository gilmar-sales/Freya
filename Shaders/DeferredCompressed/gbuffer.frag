#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in mat3 inTBN;
layout (location = 5) in vec3 inColor;
layout (location = 6) in vec2 inVelocity;
layout (location = 7) flat in uint inMaterialId;

layout (location = 0) out vec4 outAlbedo;     // RGB albedo (gamma), A matID
layout (location = 1) out vec4 outNormal;     // RGB packed normal, A 2-bit flags
layout (location = 2) out vec4 outPbr;        // R rough, G metal, B AO, A clearcoat
layout (location = 3) out vec4 outSceneColor; // HDR emissive
layout (location = 4) out vec2 outVelocity;   // UV-space motion

layout (set = 1, binding = 0) uniform sampler2D uTextures[];

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
};

layout (std430, set = 1, binding = 1) readonly buffer MaterialBuffer {
    MaterialGPU materials[];
};

// Matches historical lighting emissive boost (was applied in lighting.frag).
const float kEmissiveIntensity = 2.0;

// 2-bit flag field (packed into A2 of A2B10G10R10 as UNORM /3).
const uint kFlagReceiveShadow = 1u;
const uint kFlagIgnoreDecals  = 2u;
const uint kFlagUnlit         = 3u;

vec3 linearToSrgb(vec3 c) {
    return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 srgbToLinear(vec3 c) {
    return pow(c, vec3(2.2));
}

void main() {
    MaterialGPU mat = materials[inMaterialId];

    // Blend surfaces must not reach the G-buffer (culled via
    // kFlagTranslucent). Defensive discard if they ever do.
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

    vec3 albedoLin =
        srgbToLinear(albedoSample.rgb) * inColor *
        mat.albedoFactor.rgb;

    // Store gamma-space albedo for UNORM target; lighting applies pow(2.2).
    outAlbedo = vec4(linearToSrgb(albedoLin),
                     clamp(mat.materialId, 0.0, 255.0) / 255.0);

    uint flags = kFlagReceiveShadow;
    outNormal = vec4(worldNormal * 0.5 + 0.5, float(flags) / 3.0);

    float metalness =
        texture(uTextures[nonuniformEXT(mat.metalnessIndex)], inTexCoord).r *
        mat.roughMetal.y;
    float roughness =
        max(texture(uTextures[nonuniformEXT(mat.roughnessIndex)], inTexCoord).r *
                mat.roughMetal.x,
            0.045);
    float ao = clamp(mat.emissiveFactor.w, 0.0, 1.0);
    float clearcoat = clamp(mat.clearcoat, 0.0, 1.0);
    outPbr = vec4(roughness, metalness, ao, clearcoat);

    vec3 emissiveLin =
        srgbToLinear(
            texture(uTextures[nonuniformEXT(mat.emissiveIndex)], inTexCoord)
                .rgb) *
        mat.emissiveFactor.rgb;
    outSceneColor = vec4(emissiveLin * kEmissiveIntensity, 1.0);
    outVelocity = inVelocity;
}
