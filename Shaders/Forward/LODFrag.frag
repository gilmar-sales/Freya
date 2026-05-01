#version 450

#include "../Common/Dither.glsl"

layout (binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

// Material textures (set = 1)
layout (set = 1, binding = 0) uniform sampler2D albedoSampler;
layout (set = 1, binding = 1) uniform sampler2D normalSampler;
layout (set = 1, binding = 2) uniform sampler2D roughnessSampler;

// LOD dither texture (set = 2, binding = 0)
layout (set = 2, binding = 0) uniform sampler2D ditherSampler;

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosition;
layout (location = 2) in vec2 fragTexCoord;
layout (location = 3) in mat3 TBN;

layout (location = 0) out vec4 outColor;

// LOD fade factor (pushed as vertex attribute or as additional varying)
// For now, we calculate it from world position distance to camera
// In a full implementation, this would come from the vertex shader
layout (location = 4) in flat uint instanceId;
layout (location = 5) in float lodFadeFactor;

vec3 getNormalFromMap() {
    vec3 normal = texture(normalSampler, fragTexCoord).rgb * 2.0 - 1.0;
    return normalize(TBN * normal);
}

vec3 getCameraPosition(mat4 viewMatrix) {
    return -viewMatrix[3].xyz * mat3(viewMatrix);
}

void main()
{
    vec3 albedo = texture(albedoSampler, fragTexCoord).rgb;
    vec3 normal = getNormalFromMap();
    float roughness = texture(roughnessSampler, fragTexCoord).r;

    vec3 lightDirNorm = normalize(-pub.ambientLight.xyz);
    vec3 viewDir = normalize(getCameraPosition(pub.view) - fragPosition);

    float diff = max(dot(normal, lightDirNorm), pub.ambientLight.w);

    vec3 halfwayDir = normalize(lightDirNorm + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), mix(16.0, 2.0, roughness));

    vec3 lighting = (diff + spec) * vec3(1.0, 1.0, 1.0);
    vec3 finalColor = albedo * lighting;

    // Apply dithered LOD cross-fade
    // The lodFadeFactor should be 0..1 where 1 = fully visible, 0 = fully culled
    if (lodFadeFactor < 1.0) {
        float ditherThreshold = getDitherThreshold(gl_FragCoord.xy);
        float alpha = step(ditherThreshold, lodFadeFactor);
        outColor = vec4(finalColor, alpha);
        // Alpha blending will be handled by the render pass
    } else {
        outColor = vec4(finalColor, 1.0);
    }
}