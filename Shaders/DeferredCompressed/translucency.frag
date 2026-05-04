#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

layout (set = 1, binding = 0) uniform sampler2D albedoSampler;
layout (set = 1, binding = 1) uniform sampler2D normalSampler;
layout (set = 1, binding = 2) uniform sampler2D roughnessSampler;

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosition;
layout (location = 2) in vec2 fragTexCoord;
layout (location = 3) in vec3 fragNormal;

layout (location = 0) out vec4 outColor;

void main() {
    vec3 albedo = texture(albedoSampler, fragTexCoord).rgb;
    vec3 lightDir = normalize(-pub.ambientLight.xyz);
    vec3 N = normalize(fragNormal);
    float diff = max(dot(N, lightDir), pub.ambientLight.w);
    vec3 lighting = (diff + 0.3) * vec3(1.0, 1.0, 1.0);
    outColor = vec4(albedo * lighting, 0.5);
}
