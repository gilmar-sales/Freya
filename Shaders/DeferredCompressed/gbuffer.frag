#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in mat3 inTBN;
layout (location = 5) in vec3 inColor;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out vec4 outEmissive;
layout (location = 4) out vec4 outMaterial; // .r = metalness, .g = roughness

layout (set = 1, binding = 0) uniform sampler2D uAlbedoTexture;
layout (set = 1, binding = 1) uniform sampler2D uNormalTexture;
layout (set = 1, binding = 2) uniform sampler2D uRoughnessTexture;
layout (set = 1, binding = 3) uniform sampler2D uEmissiveTexture;
layout (set = 1, binding = 4) uniform sampler2D uMetalnessTexture;

layout (set = 1, binding = 5) uniform MaterialFactors {
    vec4 albedoFactor;
    vec4 emissiveFactor; // xyz used, w unused
    vec2 roughMetal;     // x = roughnessFactor, y = metalnessFactor
} materialFactors;

vec3 srgbToLinear(vec3 c) {
    return pow(c, vec3(2.2));
}

void main() {
    outPosition = vec4(inPosition, 1.0);

    vec3 sampled = texture(uNormalTexture, inTexCoord).rgb;
    vec3 worldNormal;
    if (all(greaterThan(sampled, vec3(0.99)))) {
        worldNormal = normalize(inTBN[2]);
    } else {
        vec3 tangentNormal = sampled * 2.0 - 1.0;
        worldNormal = normalize(inTBN * tangentNormal);
    }
    outNormal = vec4(worldNormal, 1.0);

    // Write linear albedo into sRGB G-buffer (HW encodes on store).
    vec3 albedoLin =
        srgbToLinear(texture(uAlbedoTexture, inTexCoord).rgb) * inColor *
        materialFactors.albedoFactor.rgb;
    outAlbedo = vec4(albedoLin, 1.0);

    // Emissive G-buffer is float — store linear
    outEmissive = vec4(
        srgbToLinear(texture(uEmissiveTexture, inTexCoord).rgb) *
            materialFactors.emissiveFactor.rgb,
        1.0);

    float metalness = texture(uMetalnessTexture, inTexCoord).r *
                      materialFactors.roughMetal.y;
    float roughness =
        max(texture(uRoughnessTexture, inTexCoord).r *
                materialFactors.roughMetal.x,
            0.045);
    outMaterial = vec4(metalness, roughness, 0.0, 1.0);
}
