#version 450

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inDepthBuffer;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inPosition;
layout(input_attachment_index = 2, binding = 2) uniform subpassInput inNormal;
layout(input_attachment_index = 3, binding = 3) uniform subpassInput inAlbedo;

layout(binding = 4) uniform LightBuffer {
    vec4 lightPositions[16];
    vec4 lightColorsAndRadius[16];
    vec4 lightDirectionsAndCutoff[16];
    vec4 lightOuterCutoffAndIntensity[16];
    vec4 viewPosition;
    uint lightCount;
} lights;

layout(location = 0) out vec4 outColor;

// Returns color and alpha (intensity) for a single light
vec4 calculateLight(vec3 lightPos, float lightType, vec3 lightColor, 
                   float radius, vec3 lightDir, float innerCutoff, 
                   float outerCutoff, float intensity,
                   vec3 fragPosition, vec3 N, vec3 V, float roughness, float specular) {
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

void main() {
    float depth = subpassLoad(inDepthBuffer).r;

    // Reverse-Z: depth clear is 0.0 (far plane). Pixels without geometry
    // must be skipped to avoid normalize(zero) -> NaN propagation.
    if (depth == 0.0) {
        discard;
    }

    vec3 fragPos = subpassLoad(inPosition).xyz;
    vec3 normal = subpassLoad(inNormal).xyz;
    vec4 albedo = subpassLoad(inAlbedo);

    float specular = albedo.a;

    vec3 N = normalize(normal);
    vec3 V = normalize(lights.viewPosition.xyz - fragPos);

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
                fragPos, N, V, albedo.a, specular
            );
            totalLighting += lightResult.rgb;
            totalIntensity += lightResult.a;
        }
    } else {
        // Fallback: hardcoded directional light for backward compatibility
        vec3 lightDir = normalize(vec3(0.0, -3.0, -1.0));
        float ambientIntensity = 0.5;
        float lightIntensity = 0.5;
        
        float diff = max(dot(N, -lightDir), 0.0) * lightIntensity + ambientIntensity;
        
        vec3 halfwayDir = normalize(-lightDir + V);
        float spec = pow(max(dot(N, halfwayDir), 0.0), 32.0) * specular;
        
        totalLighting = vec3(1.0, 1.0, 1.0) * (diff + spec);
    }

    outColor = vec4(albedo.rgb * totalLighting, 1.0);
}