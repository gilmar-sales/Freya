#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in mat3 inTBN;
layout (location = 5) in vec3 inColor;
layout (location = 6) in vec2 inVelocity;

layout (location = 0) out vec4 outAlbedo;     // RGB albedo (gamma), A matID
layout (location = 1) out vec4 outNormal;     // RGB packed normal, A 2-bit flags
layout (location = 2) out vec4 outPbr;        // R rough, G metal, B AO, A free
layout (location = 3) out vec4 outSceneColor; // HDR emissive
layout (location = 4) out vec2 outVelocity;   // UV-space motion

layout (set = 1, binding = 0) uniform sampler2D uAlbedoTexture;
layout (set = 1, binding = 1) uniform sampler2D uNormalTexture;
layout (set = 1, binding = 2) uniform sampler2D uRoughnessTexture;
layout (set = 1, binding = 3) uniform sampler2D uEmissiveTexture;
layout (set = 1, binding = 4) uniform sampler2D uMetalnessTexture;

layout (set = 1, binding = 5) uniform MaterialFactors {
    vec4 albedoFactor;
    vec4 emissiveFactor; // xyz emissive, w = aoFactor
    vec2 roughMetal;     // x = roughnessFactor, y = metalnessFactor
    float materialId;    // 0–255
    float alphaCutoff;   // 0 = cutout disabled
} materialFactors;

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
    vec4 albedoSample = texture(uAlbedoTexture, inTexCoord);
    float alpha = albedoSample.a * materialFactors.albedoFactor.a;
    if (materialFactors.alphaCutoff > 0.0 && alpha < materialFactors.alphaCutoff)
        discard;

    vec3 sampled = texture(uNormalTexture, inTexCoord).rgb;
    vec3 worldNormal;
    if (all(greaterThan(sampled, vec3(0.99)))) {
        worldNormal = normalize(inTBN[2]);
    } else {
        vec3 tangentNormal = sampled * 2.0 - 1.0;
        worldNormal = normalize(inTBN * tangentNormal);
    }

    vec3 albedoLin =
        srgbToLinear(albedoSample.rgb) * inColor *
        materialFactors.albedoFactor.rgb;

    // Store gamma-space albedo for UNORM target; lighting applies pow(2.2).
    outAlbedo = vec4(linearToSrgb(albedoLin),
                     clamp(materialFactors.materialId, 0.0, 255.0) / 255.0);

    uint flags = kFlagReceiveShadow;
    outNormal = vec4(worldNormal * 0.5 + 0.5, float(flags) / 3.0);

    float metalness = texture(uMetalnessTexture, inTexCoord).r *
                      materialFactors.roughMetal.y;
    float roughness =
        max(texture(uRoughnessTexture, inTexCoord).r *
                materialFactors.roughMetal.x,
            0.045);
    float ao = clamp(materialFactors.emissiveFactor.w, 0.0, 1.0);
    float free = 0.0;
    outPbr = vec4(roughness, metalness, ao, free);

    vec3 emissiveLin =
        srgbToLinear(texture(uEmissiveTexture, inTexCoord).rgb) *
        materialFactors.emissiveFactor.rgb;
    outSceneColor = vec4(emissiveLin * kEmissiveIntensity, 1.0);
    outVelocity = inVelocity;
}
