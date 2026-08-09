#version 450
#extension GL_EXT_nonuniform_qualifier : require

// Weighted Blended OIT accumulate with analytical lights (no shadows/IBL).

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in vec3 inColor;
layout (location = 3) flat in uint inMaterialId;
layout (location = 4) in float inViewZ;
layout (location = 5) in vec3 inWorldNormal;

layout (location = 0) out vec4 outAccum;
layout (location = 1) out float outReveal;

layout (set = 0, binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
    vec4 _pad0;
    vec4 _pad1;
    vec4 _pad2;
    mat4 invViewProjection;
    mat4 prevViewProjection;
    mat4 unjitteredProjection;
} pub;

layout (set = 1, binding = 0) uniform sampler2D uTextures[];

struct MaterialGPU {
    uint albedoIndex;
    uint normalIndex;
    uint roughnessIndex;
    uint emissiveIndex;
    uint metalnessIndex;
    uint alphaMode;
    uint _pad1;
    uint _pad2;
    vec4 albedoFactor;
    vec4 emissiveFactor;
    vec2 roughMetal;
    float materialId;
    float alphaCutoff;
};

layout (std430, set = 1, binding = 1) readonly buffer MaterialBuffer {
    MaterialGPU materials[];
};

layout (set = 2, binding = 0) uniform LightBuffer {
    vec4 lightPositions[16];
    vec4 lightColorsAndRadius[16];
    vec4 lightDirectionsAndCutoff[16];
    vec4 lightOuterCutoffAndIntensity[16];
    vec4 lightAreaTangents[16];
    vec4 viewPosition;
    vec4 cameraForward;
    uint lightCount;
    float iblIntensity;
    float exposure;
} lights;

const float PI = 3.14159265359;
const float kEmissiveIntensity = 1.25;

vec3 srgbToLinear(vec3 c) {
    return pow(c, vec3(2.2));
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 1e-4);
}

float GeometrySchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 1e-4);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void resolveLight(vec3 lightPos, float lightType, float radius, vec3 lightDir,
                  float innerCutoff, float outerCutoff, vec3 worldPos,
                  out vec3 L, out float attenuation) {
    L = vec3(0.0);
    attenuation = 0.0;

    if (lightType < 0.5) {
        // Point (area approximated as point at center)
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        L = toLight / max(dist, 1e-4);
        attenuation = radius / (dist * dist + 1.0);
    } else if (lightType < 1.5) {
        // Directional
        L = -normalize(lightDir);
        attenuation = 1.0;
    } else if (lightType < 2.5) {
        // Spot
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        L = toLight / max(dist, 1e-4);
        float spotCos = dot(L, -normalize(lightDir));
        float spotFactor = smoothstep(outerCutoff, innerCutoff, spotCos);
        attenuation = spotFactor * radius / (dist * dist + 1.0);
    } else {
        // Area → treat as point at center
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        L = toLight / max(dist, 1e-4);
        attenuation = 8.0 / (dist * dist + 1.0);
    }
}

vec3 shadeLight(vec3 lightPos, float lightType, vec3 lightColor, float radius,
                vec3 lightDir, float innerCutoff, float outerCutoff,
                float intensity, vec3 worldPos, vec3 N, vec3 V, vec3 albedo,
                float roughness, float metalness, vec3 F0) {
    vec3 L;
    float attenuation;
    resolveLight(lightPos, lightType, radius, lightDir, innerCutoff,
                 outerCutoff, worldPos, L, attenuation);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0 || attenuation <= 0.0)
        return vec3(0.0);

    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular =
        (NDF * G * F) / max(4.0 * max(dot(N, V), 0.0) * NdotL, 1e-4);

    // Glass: little Lambert diffuse; transmission tint is albedo*alpha in WBOIT.
    vec3 kD = (vec3(1.0) - F) * (1.0 - metalness) * 0.15;
    vec3 radiance = lightColor * intensity * attenuation;
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

float wboitWeight(float z, float a) {
    return clamp(a * max(1e-2, 3e3 * pow(1.0 - z / 200.0, 3.0)), 1e-2, 3e3);
}

void main() {
    MaterialGPU mat = materials[inMaterialId];

    vec4 albedoSample =
        texture(uTextures[nonuniformEXT(mat.albedoIndex)], inTexCoord);
    float baseAlpha = clamp(albedoSample.a * mat.albedoFactor.a, 0.0, 1.0);
    if (baseAlpha < 1e-4)
        discard;

    vec3 albedoLin =
        srgbToLinear(albedoSample.rgb) * inColor * mat.albedoFactor.rgb;
    vec3 emissiveLin =
        srgbToLinear(
            texture(uTextures[nonuniformEXT(mat.emissiveIndex)], inTexCoord)
                .rgb) *
        mat.emissiveFactor.rgb * kEmissiveIntensity;

    float roughness = clamp(mat.roughMetal.x, 0.04, 1.0);
    float metalness = clamp(mat.roughMetal.y, 0.0, 1.0);

    vec3 N = normalize(inWorldNormal);
    vec3 V = normalize(lights.viewPosition.xyz - inWorldPos);
    if (dot(N, V) < 0.0)
        N = -N;

    // Dielectric glass F0; metals still work via metalness.
    vec3 F0 = mix(vec3(0.04), albedoLin, metalness);
    float NdotV = max(dot(N, V), 0.0);
    vec3 Fr = fresnelSchlick(NdotV, F0);

    vec3 ambient = pub.ambientLight.rgb * pub.ambientLight.a * albedoLin * 0.25;
    vec3 lit = ambient;
    uint count = min(lights.lightCount, 16u);
    for (uint i = 0u; i < count; ++i) {
        lit += shadeLight(
            lights.lightPositions[i].xyz, lights.lightPositions[i].w,
            lights.lightColorsAndRadius[i].rgb,
            lights.lightColorsAndRadius[i].w,
            lights.lightDirectionsAndCutoff[i].xyz,
            lights.lightDirectionsAndCutoff[i].w,
            lights.lightOuterCutoffAndIntensity[i].x,
            lights.lightOuterCutoffAndIntensity[i].y, inWorldPos, N, V,
            albedoLin, roughness, metalness, F0);
    }

    // Transmission body + specular/Fresnel highlights + filament.
    vec3 C = lit * (1.0 - Fr) + lit * Fr * 2.5 + emissiveLin;

    // Edges / highlights write more coverage so specular isn't washed out.
    float fresnelCoverage = max(Fr.r, max(Fr.g, Fr.b));
    float alpha = clamp(max(baseAlpha, fresnelCoverage * 0.85), 0.0, 1.0);

    float w = wboitWeight(max(inViewZ, 1e-3), alpha);
    outAccum = vec4(C * alpha * w, alpha * w);
    outReveal = alpha;
}
