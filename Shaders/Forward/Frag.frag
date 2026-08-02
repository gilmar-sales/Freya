#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

layout(binding = 1) uniform LightBuffer {
    vec4 lightPositions[16];
    vec4 lightColorsAndRadius[16];
    vec4 lightDirectionsAndCutoff[16];
    vec4 lightOuterCutoffAndIntensity[16];
    vec4 viewPosition;
    uint lightCount;
} lights;

layout(set = 1, binding = 0) uniform sampler2D albedoSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;
layout(set = 1, binding = 2) uniform sampler2D roughnessSampler;
layout(set = 1, binding = 3) uniform sampler2D emissiveSampler;
layout(set = 1, binding = 4) uniform sampler2D metalnessSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosition;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in mat3 TBN;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

vec3 getNormalFromMap() {
    vec3 normal = texture(normalSampler, fragTexCoord).rgb * 2.0 - 1.0;
    return normalize(TBN * normal);
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

// Resolve light direction + attenuation for Point / Directional / Spot.
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

// Cook-Torrance GGX contribution for one light (albedo already applied).
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

void main()
{
    vec3 albedo = texture(albedoSampler, fragTexCoord).rgb;
    vec3 normal = getNormalFromMap();
    float roughness =
        max(texture(roughnessSampler, fragTexCoord).r, 0.045);
    float metalness = texture(metalnessSampler, fragTexCoord).r;
    vec3 emissive = texture(emissiveSampler, fragTexCoord).rgb;

    vec3 N = normalize(normal);
    vec3 V = normalize(lights.viewPosition.xyz - fragPosition);
    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    vec3 totalLighting = vec3(0.0);

    if (lights.lightCount > 0) {
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
                fragPosition, N, V, albedo, roughness, metalness, F0);
        }
    } else {
        // Fallback: treat ambientLight as a dim directional + ambient fill
        totalLighting += calculateLight(
            vec3(0.0), 1.0, vec3(1.0), 1.0, pub.ambientLight.xyz, 0.0, 0.0,
            1.0, fragPosition, N, V, albedo, roughness, metalness, F0);
        totalLighting +=
            albedo * pub.ambientLight.w * (1.0 - metalness);
    }

    outColor = vec4(totalLighting + emissive, 1.0);
}
