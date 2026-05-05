#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

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

void main() {
    outPosition = vec4(inPosition, 1.0);

    vec3 N = normalize(inNormal);
    N.y = -N.y;
    outNormal = vec4(N, 1.0);

    vec4 albedo = texture(uAlbedoTexture, inTexCoord);
    float roughness = texture(uRoughnessTexture, inTexCoord).r;
    outAlbedo = vec4(albedo.rgb, roughness);
    
    // Emissive stored in RGB, defaults to black if no emissive texture
    outEmissive = vec4(texture(uEmissiveTexture, inTexCoord).rgb, 1.0);
    
    // Metalness defaults to 0.0, roughness defaults to 0.5
    float metalness = texture(uMetalnessTexture, inTexCoord).r;
    outMaterial = vec4(metalness, roughness, 0.0, 1.0);
}
