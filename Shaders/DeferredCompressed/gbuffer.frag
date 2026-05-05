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

    // Sample and decode normal from normal map
    vec3 normalTex = texture(uNormalTexture, inTexCoord).rgb * 2.0 - 1.0;
    
    // Compute TBN matrix from derivatives (no tangent attribute available)
    vec3 Q1 = dFdx(outPosition.xyz);
    vec3 Q2 = dFdy(outPosition.xyz);
    vec2 UV1 = dFdx(inTexCoord);
    vec2 UV2 = dFdy(inTexCoord);
    
    vec3 T = normalize(Q1 * UV2.y - Q2 * UV1.y);
    vec3 B = normalize(Q2 * UV1.x - Q1 * UV2.x);
    vec3 N_for_TBN = normalize(N);
    
    mat3 TBN = mat3(T, B, N_for_TBN);
    vec3 worldNormal = normalize(TBN * normalTex);
    
    outNormal = vec4(worldNormal, 1.0);

    vec4 albedo = texture(uAlbedoTexture, inTexCoord);
    float roughness = texture(uRoughnessTexture, inTexCoord).r;
    outAlbedo = vec4(albedo.rgb, roughness);
    
    // Emissive stored in RGB, defaults to black if no emissive texture
    outEmissive = vec4(texture(uEmissiveTexture, inTexCoord).rgb, 1.0);
    
    // Metalness defaults to 0.0, roughness defaults to 0.5
    float metalness = texture(uMetalnessTexture, inTexCoord).r;
    outMaterial = vec4(metalness, roughness, 0.0, 1.0);
}
