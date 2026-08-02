#version 450

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inDepthBuffer;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inPosition;
layout(input_attachment_index = 2, binding = 2) uniform subpassInput inNormal;
layout(input_attachment_index = 3, binding = 3) uniform subpassInput inAlbedo;
layout(input_attachment_index = 4, binding = 4) uniform subpassInput inEmissive;
layout(input_attachment_index = 5, binding = 5) uniform subpassInput inMaterial;

layout(binding = 6) uniform LightBuffer {
    vec4 lightPositions[16];
    vec4 lightColorsAndRadius[16];
    vec4 lightDirectionsAndCutoff[16];
    vec4 lightOuterCutoffAndIntensity[16];
    vec4 viewPosition;
    uint lightCount;
    float iblIntensity;
} lights;

layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D prefilterMap;
layout(binding = 9) uniform sampler2D brdfLUT;

layout(location = 0) out vec4 outColor;

const float bloomIntensity = 2.0;
const float PI = 3.14159265359;

vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
    uv *= vec2(0.15915494309, 0.31830988618);
    uv += 0.5;
    return uv;
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 1e-4);
}

float GeometrySchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 1e-4);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) *
           GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
                    pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void resolveLight(vec3 lightPos, float lightType, float radius, vec3 lightDir,
                  float innerCutoff, float outerCutoff, vec3 worldPos,
                  out vec3 L, out float attenuation) {
    L = vec3(0.0);
    attenuation = 1.0;

    if (lightType < 0.5) {
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        L = normalize(toLight);
        attenuation = radius / (dist * dist + 1.0);
    } else if (lightType < 1.5) {
        L = -normalize(lightDir);
        attenuation = 1.0;
    } else {
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        L = normalize(toLight);

        float spotCos = dot(L, -normalize(lightDir));
        float spotFactor = smoothstep(outerCutoff, innerCutoff, spotCos);
        attenuation = spotFactor * radius / (dist * dist + 1.0);
    }
}

vec3 calculateLight(vec3 lightPos, float lightType, vec3 lightColor,
                    float radius, vec3 lightDir, float innerCutoff,
                    float outerCutoff, float intensity, vec3 worldPos,
                    vec3 N, vec3 V, vec3 albedo, float roughness,
                    float metalness, vec3 F0) {
    vec3 L;
    float attenuation;
    resolveLight(lightPos, lightType, radius, lightDir, innerCutoff,
                 outerCutoff, worldPos, L, attenuation);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0 || attenuation <= 0.0) {
        return vec3(0.0);
    }

    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular =
        (NDF * G * F) /
        max(4.0 * max(dot(N, V), 0.0) * NdotL, 1e-4);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);

    vec3 radiance = lightColor * intensity * attenuation;
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec3 calculateIBL(vec3 N, vec3 V, vec3 albedo, float roughness, float metalness,
                  vec3 F0) {
    float NdotV = max(dot(N, V), 0.0);
    vec3 R = reflect(-V, N);

    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metalness);

    vec3 irradiance = texture(irradianceMap, SampleSphericalMap(N)).rgb;
    vec3 diffuse = irradiance * albedo;

    float maxLod = float(textureQueryLevels(prefilterMap) - 1);
    vec3 prefilteredColor =
        textureLod(prefilterMap, SampleSphericalMap(R), roughness * maxLod)
            .rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    return (kD * diffuse + specular) * lights.iblIntensity;
}

void main() {
    float depth = subpassLoad(inDepthBuffer).r;

    if (depth == 0.0) {
        discard;
    }

    vec3 fragPos = subpassLoad(inPosition).xyz;
    vec3 normal = subpassLoad(inNormal).xyz;
    vec4 albedoSample = subpassLoad(inAlbedo);
    vec3 emissive = subpassLoad(inEmissive).rgb;
    vec4 material = subpassLoad(inMaterial);

    vec3 albedo = albedoSample.rgb;
    float metalness = material.r;
    float roughness = max(material.g, 0.045);

    vec3 N = normalize(normal);
    vec3 V = normalize(lights.viewPosition.xyz - fragPos);
    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    vec3 totalLighting = calculateIBL(N, V, albedo, roughness, metalness, F0);

    for (int i = 0; i < int(lights.lightCount); i++) {
        totalLighting += calculateLight(
            lights.lightPositions[i].xyz,
            lights.lightPositions[i].w,
            lights.lightColorsAndRadius[i].rgb,
            lights.lightColorsAndRadius[i].w,
            lights.lightDirectionsAndCutoff[i].xyz,
            lights.lightDirectionsAndCutoff[i].w,
            lights.lightOuterCutoffAndIntensity[i].x,
            lights.lightOuterCutoffAndIntensity[i].y,
            fragPos, N, V, albedo, roughness, metalness, F0);
    }

    outColor =
        vec4(totalLighting + emissive * bloomIntensity, 1.0);
}
