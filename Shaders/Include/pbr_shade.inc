#ifndef PBR_SHADE_INC
#define PBR_SHADE_INC

#include "pbr_brdf.inc"
#include "pbr_light.inc"

#ifndef PBR_DIFFUSE_SCALE
#define PBR_DIFFUSE_SCALE 1.0
#endif

vec3 calculateLight(int lightIndex, vec3 lightPos, float lightType,
                    vec3 lightColor, float radius, vec3 lightDir,
                    float innerCutoff, float outerCutoff, float intensity,
                    vec3 worldPos, vec3 N, vec3 V, vec3 albedo,
                    float roughness, float metalness, vec3 F0,
                    bool receiveShadow, float coatSpecAtten) {
    vec3 L;
    float attenuation;
    resolveLight(lightPos, lightType, radius, lightDir, innerCutoff,
                 outerCutoff, worldPos, L, attenuation);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0 || attenuation <= 0.0) {
        return vec3(0.0);
    }

#ifdef PBR_WITH_SHADOWS
    float shadowFactor =
        GetShadowFactor(lightIndex, lightType, worldPos, N, L, receiveShadow);
#else
    float shadowFactor = 1.0;
#endif

    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular =
        (NDF * G * F) /
        max(4.0 * max(dot(N, V), 0.0) * NdotL, 1e-4);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness) * PBR_DIFFUSE_SCALE;

    vec3 radiance = lightColor * intensity * attenuation;
    return (kD * albedo / PI + specular * coatSpecAtten) * radiance * NdotL *
           shadowFactor;
}

vec3 calculateClearcoatLight(vec3 lightPos, float lightType, vec3 lightColor,
                             float radius, vec3 lightDir, float innerCutoff,
                             float outerCutoff, float intensity, vec3 worldPos,
                             vec3 N, vec3 V, float coatRoughness,
                             float clearcoat) {
    vec3 L;
    float attenuation;
    resolveLight(lightPos, lightType, radius, lightDir, innerCutoff,
                 outerCutoff, worldPos, L, attenuation);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0 || attenuation <= 0.0 || clearcoat < 1e-3)
        return vec3(0.0);

    vec3 H = normalize(V + L);
    vec3 F0c = kDielectricF0;
    float NDF = DistributionGGX(N, H, coatRoughness);
    float G = GeometrySmith(N, V, L, coatRoughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0c);
    vec3 specular =
        (NDF * G * F) /
        max(4.0 * max(dot(N, V), 0.0) * NdotL, 1e-4);
    vec3 radiance = lightColor * intensity * attenuation;
    return specular * radiance * NdotL * clearcoat;
}

#endif
