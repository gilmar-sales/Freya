#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
} pub;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosition;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    float lightIntensity = pub.ambientLight.w + max(dot(normalize(fragNormal), pub.ambientLight.xyz), 0);
    
    outColor = vec4(lightIntensity * fragColor + texture(texSampler, fragTexCoord).xyz, 1.0);
}