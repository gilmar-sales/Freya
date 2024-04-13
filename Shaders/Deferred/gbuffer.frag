#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;

layout (set=0, binding=1) uniform sampler2D uTexture;

void main() {
    outAlbedo = texture(uTexture, inTexCoord);
    
    vec3 normal = normalize(inNormal);
    outNormal = vec4(0.5 * normal + 0.5, 1.0);
}