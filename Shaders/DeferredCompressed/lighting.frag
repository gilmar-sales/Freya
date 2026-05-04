#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inDepthBuffer;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inPosition;
layout (input_attachment_index = 2, binding = 2) uniform subpassInput inNormal;
layout (input_attachment_index = 3, binding = 3) uniform subpassInput inAlbedo;

layout (location = 0) out vec4 outColor;
layout (location = 0) in vec2 inUV;

void main() {
    vec3 fragPos = subpassLoad(inPosition).xyz;
    vec3 normal = subpassLoad(inNormal).xyz;
    vec4 albedo = subpassLoad(inAlbedo);

    float specular = albedo.a;

    // Directional light
    vec3 lightDir = normalize(vec3(0.0, -3.0, -1.0));
    float ambientIntensity = 0.05;
    float lightIntensity = 0.5;

    vec3 N = normalize(normal);
    float diff = max(dot(N, -lightDir), 0.0) * lightIntensity + ambientIntensity;

    // Specular (Blinn-Phong)
    vec3 viewDir = normalize(-fragPos);
    vec3 halfwayDir = normalize(-lightDir + viewDir);
    float spec = pow(max(dot(N, halfwayDir), 0.0), 32.0) * specular;

    vec3 finalColor = albedo.rgb * diff + vec3(spec);
    outColor = vec4(finalColor, 1.0);
}
