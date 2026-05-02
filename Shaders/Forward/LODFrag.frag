#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "../Common/Dither.glsl"

layout (binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

// Bindless texture arrays (set 1) — indexed by materialId
layout (set = 1, binding = 0) uniform sampler2D albedoTextures[];
layout (set = 1, binding = 1) uniform sampler2D normalTextures[];
layout (set = 1, binding = 2) uniform sampler2D roughnessTextures[];

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosition;
layout (location = 2) in vec2 fragTexCoord;
layout (location = 3) in mat3 TBN;

// LOD fade factor
layout (location = 9) in flat uint materialId;
layout (location = 6) in float lodFadeFactor;

layout (location = 0) out vec4 outColor;

vec3 getNormalFromMap(sampler2D normalMap, vec2 uv) {
    vec3 normal = texture(normalMap, uv).rgb * 2.0 - 1.0;
    return normalize(TBN * normal);
}

vec3 getCameraPosition(mat4 viewMatrix) {
    return -viewMatrix[3].xyz * mat3(viewMatrix);
}

void main()
{
    uint matId = nonuniformEXT(materialId);
    vec3 albedo = texture(albedoTextures[matId], fragTexCoord).rgb;
    vec3 normal = getNormalFromMap(normalTextures[matId], fragTexCoord);
    float roughness = texture(roughnessTextures[matId], fragTexCoord).r;

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
