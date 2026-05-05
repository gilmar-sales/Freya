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

vec3 getNormalFromMap() {
    vec3 normal = texture(normalSampler, fragTexCoord).rgb * 2.0 - 1.0;
    return normalize(TBN * normal);
}

vec3 getCameraPosition(mat4 viewMatrix) {
    return -viewMatrix[3].xyz * mat3(viewMatrix);
}

// Returns color and alpha (intensity) for a single light
vec4 calculateLight(vec3 lightPos, float lightType, vec3 lightColor, 
                   float radius, vec3 lightDir, float innerCutoff, 
                   float outerCutoff, float intensity,
                   vec3 fragPosition, vec3 N, vec3 V, float roughness) {
    vec3 L = vec3(0.0);
    float attenuation = 1.0;
    
    if (lightType < 0.5) {
        // Point light
        vec3 toLight = lightPos - fragPosition;
        float dist = length(toLight);
        L = normalize(toLight);
        attenuation = radius / (dist * dist + 1.0);
    } else if (lightType < 1.5) {
        // Directional light
        L = -normalize(lightDir);
        attenuation = 1.0;
    } else {
        // Spot light
        vec3 toLight = lightPos - fragPosition;
        float dist = length(toLight);
        L = normalize(toLight);
        
        float spotCos = dot(L, -normalize(lightDir));
        float spotCutoff = innerCutoff; // cosine stored pre-computed
        float spotOuter = outerCutoff;
        
        // smoothstep gives smooth falloff between outer and inner cutoff
        float spotFactor = smoothstep(spotOuter, spotCutoff, spotCos);
        attenuation = spotFactor * radius / (dist * dist + 1.0);
    }
    
    // Diffuse (Lambert)
    float diff = max(dot(N, L), 0.0);
    
    // Specular (Blinn-Phong)
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), mix(16.0, 2.0, roughness));
    
    // Combine with light color and intensity
    float lightingFactor = (diff + spec) * intensity * attenuation;
    return vec4(lightColor * lightingFactor, intensity * attenuation);
}

void main()
{
    vec3 albedo = texture(albedoSampler, fragTexCoord).rgb;
    vec3 normal = getNormalFromMap();
    float roughness = texture(roughnessSampler, fragTexCoord).r;
    vec3 emissive = texture(emissiveSampler, fragTexCoord).rgb;

    vec3 viewDir = normalize(lights.viewPosition.xyz - fragPosition);

    vec3 totalLighting = vec3(0.0);
    float totalIntensity = 0.0;
    
    // Check if we have lights defined
    if (lights.lightCount > 0) {
        // Iterate through all lights
        for (int i = 0; i < int(lights.lightCount); i++) {
            vec4 lightResult = calculateLight(
                lights.lightPositions[i].xyz,
                lights.lightPositions[i].w,
                lights.lightColorsAndRadius[i].rgb,
                lights.lightColorsAndRadius[i].w,
                lights.lightDirectionsAndCutoff[i].xyz,
                lights.lightDirectionsAndCutoff[i].w,
                lights.lightOuterCutoffAndIntensity[i].x,
                lights.lightOuterCutoffAndIntensity[i].y,
                fragPosition, normal, viewDir, roughness
            );
            totalLighting += lightResult.rgb;
            totalIntensity += lightResult.a;
        }
    } else {
        // Fallback: use ambientLight from ProjectionUniformBuffer for 
        // backward compatibility
        vec3 lightDirNorm = normalize(-pub.ambientLight.xyz);
        
        float diff = max(dot(normal, lightDirNorm), pub.ambientLight.w);
        
        vec3 halfwayDir = normalize(lightDirNorm + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 
                         mix(16.0, 2.0, roughness));
        
        totalLighting = vec3(1.0, 1.0, 1.0) * (diff + spec);
    }

    outColor = vec4(albedo * totalLighting + emissive, 1.0);
}