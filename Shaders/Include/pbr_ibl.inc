#ifndef PBR_IBL_INC
#define PBR_IBL_INC

#include "pbr_brdf.inc"

// Requires sampler2D prefilterMap, brdfLUT, and UBO `lights.iblIntensity`.
// Diffuse irradiance: define FREYA_IBL_SH and declare `vec4 irradianceSh[9]`
// (order-2 SH of the irradiance map, see src/Freya/Internal/IrradianceSh.hpp)
// to skip the equirect fetch; otherwise sampler2D irradianceMap is read.

vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
    uv *= vec2(0.15915494309, 0.31830988618);
    uv += 0.5;
    return uv;
}

#ifdef FREYA_IBL_SH
vec3 EvalIrradianceSh(vec3 n) {
    vec3 e = irradianceSh[0].rgb * 0.282095 +
             irradianceSh[1].rgb * (0.488603 * n.y) +
             irradianceSh[2].rgb * (0.488603 * n.z) +
             irradianceSh[3].rgb * (0.488603 * n.x) +
             irradianceSh[4].rgb * (1.092548 * n.x * n.y) +
             irradianceSh[5].rgb * (1.092548 * n.y * n.z) +
             irradianceSh[6].rgb * (0.315392 * (3.0 * n.z * n.z - 1.0)) +
             irradianceSh[7].rgb * (1.092548 * n.x * n.z) +
             irradianceSh[8].rgb * (0.546274 * (n.x * n.x - n.y * n.y));
    return max(e, vec3(0.0));
}
#endif

vec3 calculateIBL(vec3 N, vec3 V, vec3 albedo, float roughness, float metalness,
                  vec3 F0, float coatSpecAtten) {
    float NdotV = max(dot(N, V), 0.0);
    vec3 R = reflect(-V, N);

    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metalness);

#ifdef FREYA_IBL_SH
    vec3 irradiance = EvalIrradianceSh(N);
#else
    vec3 irradiance = texture(irradianceMap, SampleSphericalMap(N)).rgb;
#endif
    vec3 diffuse = irradiance * albedo;

    float maxLod = float(textureQueryLevels(prefilterMap) - 1);
    vec3 prefilteredColor =
        textureLod(prefilterMap, SampleSphericalMap(R), roughness * maxLod)
            .rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y) * coatSpecAtten;

    return (kD * diffuse + specular) * lights.iblIntensity;
}

vec3 calculateClearcoatIBL(vec3 N, vec3 V, float coatRoughness,
                           float clearcoat) {
    if (clearcoat < 1e-3)
        return vec3(0.0);
    float NdotV = max(dot(N, V), 0.0);
    vec3 R = reflect(-V, N);
    vec3 F0c = kDielectricF0;
    vec3 F = fresnelSchlickRoughness(NdotV, F0c, coatRoughness);
    float maxLod = float(textureQueryLevels(prefilterMap) - 1);
    vec3 prefilteredColor =
        textureLod(prefilterMap, SampleSphericalMap(R),
                   coatRoughness * maxLod)
            .rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, coatRoughness)).rg;
    return prefilteredColor * (F * brdf.x + brdf.y) * clearcoat *
           lights.iblIntensity;
}

#endif
