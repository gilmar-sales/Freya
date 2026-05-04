#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;

layout (set = 1, binding = 0) uniform sampler2D uAlbedoTexture;
layout (set = 1, binding = 1) uniform sampler2D uNormalTexture;
layout (set = 1, binding = 2) uniform sampler2D uSpecularTexture;

void main() {
    outPosition = vec4(inPosition, 1.0);

    vec3 N = normalize(inNormal);
    N.y = -N.y;
    outNormal = vec4(N, 1.0);

    vec4 albedo = texture(uAlbedoTexture, inTexCoord);
    float specular = texture(uSpecularTexture, inTexCoord).r;
    outAlbedo = vec4(albedo.rgb, specular);
}
