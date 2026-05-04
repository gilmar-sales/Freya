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

layout (location = 0) out vec3 fragColor;
layout (location = 1) out vec3 fragPosition;
layout (location = 2) out vec2 fragTexCoord;
layout (location = 3) out vec3 fragNormal;

void main() {
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    gl_Position = pub.proj * pub.view * worldPos;

    fragColor = inColor;
    fragPosition = worldPos.xyz;
    fragTexCoord = inTexCoord;

    mat3 mNormal = transpose(inverse(mat3(inModel)));
    fragNormal = mNormal * normalize(inNormal);
}
