#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 1, binding = 0) uniform sampler2D uTextures[];

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) flat in uint vTextureIndex;
layout(location = 3) in float vClipU;
layout(location = 4) in float vClipMax;

layout(location = 0) out vec4 outColor;

void main()
{
    if (vClipU > vClipMax)
        discard;

    vec4 texel = texture(uTextures[nonuniformEXT(vTextureIndex)], vUv);
    vec4 color = texel * vColor;
    if (color.a < 0.001)
        discard;
    outColor = color;
}
