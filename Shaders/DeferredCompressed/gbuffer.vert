#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec2 inTexCoord;
layout (location = 5) in mat4 inModel;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec2 outTexCoord;
layout (location = 2) out mat3 outTBN;
layout (location = 5) out vec3 outColor;

void main() {
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    gl_Position = pub.proj * pub.view * worldPos;
    outPosition = worldPos.xyz;
    outTexCoord = inTexCoord;
    outColor = inColor;

    // Match Forward/Vert.vert so normal maps share the same
    // world-space basis (derivative TBN often flips B under Vulkan).
    vec3 T = normalize(vec3(inModel * vec4(inTangent, 0.0)));
    vec3 N = normalize(vec3(inModel * vec4(inNormal, 0.0)));
    T = normalize(T - N * dot(N, T));
    vec3 B = cross(N, T);

    outTBN = mat3(T, B, N);
}
