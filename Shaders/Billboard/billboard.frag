#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 1, binding = 0) uniform sampler2D uTextures[];

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) flat in uint vTextureIndex;
layout(location = 3) in float vClipU;
layout(location = 4) in float vClipMax;
layout(location = 5) flat in uint vFlags;

layout(location = 0) out vec4 outColor;

const uint kSdf = 2u;

void main()
{
    if (vClipU > vClipMax)
        discard;

    vec4 texel = texture(uTextures[nonuniformEXT(vTextureIndex)], vUv);
    vec4 color;
    if ((vFlags & kSdf) != 0u)
    {
        float d     = texel.r;
        float width = fwidth(d);
        float alpha = smoothstep(0.5 - width, 0.5 + width, d);
        color       = vec4(vColor.rgb, vColor.a * alpha);
    }
    else
    {
        color = texel * vColor;
    }
    if (color.a < 0.001)
        discard;
    outColor = color;
}
