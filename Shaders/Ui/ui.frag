#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 1, binding = 0) uniform sampler2D uTextures[];

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) flat in uint vTextureIndex;
layout(location = 3) in float vClipU;
layout(location = 4) in float vClipMax;
layout(location = 5) flat in uint vFlags;
layout(location = 6) in vec4 vOutlineColor;
layout(location = 7) in float vOutlineWidth;
layout(location = 8) in vec2 vSizePx;
layout(location = 9) in float vRounding;
layout(location = 10) in float vBorderWidth;
layout(location = 11) in vec2 vUv01;

layout(location = 0) out vec4 outColor;

const uint kSdfGlyph   = 1u;
const uint kSdfRounded = 2u;
const uint kClipU      = 4u;

float sdRoundedBox(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
    if ((vFlags & kClipU) != 0u && vClipU > vClipMax)
        discard;

    vec4 color;

    if ((vFlags & kSdfGlyph) != 0u)
    {
        vec4  texel = texture(uTextures[nonuniformEXT(vTextureIndex)], vUv);
        float d     = texel.r;
        float fw    = max(fwidth(d), 1e-5);
        float fill  = smoothstep(0.5 - fw, 0.5 + fw, d);
        if (vOutlineWidth < 1e-4)
        {
            color = vec4(vColor.rgb, vColor.a * fill);
        }
        else
        {
            float edge = 0.5 - clamp(vOutlineWidth, 0.0, 0.49);
            float ring = smoothstep(edge - fw, edge + fw, d);
            color      = mix(vOutlineColor, vColor, fill);
            color.a *= ring;
        }
    }
    else if ((vFlags & kSdfRounded) != 0u)
    {
        vec2  halfSize = max(vSizePx * 0.5, vec2(1e-4));
        vec2  p        = (vUv01 - vec2(0.5)) * vSizePx;
        float r        = clamp(vRounding, 0.0, min(halfSize.x, halfSize.y));
        float dist     = sdRoundedBox(p, halfSize, r);
        float fw       = max(fwidth(dist), 1e-4);
        float outer    = 1.0 - smoothstep(-fw, fw, dist);

        if (vBorderWidth > 1e-4)
        {
            float bw = min(vBorderWidth, min(halfSize.x, halfSize.y));
            float innerDist =
                sdRoundedBox(p, halfSize - vec2(bw), max(r - bw, 0.0));
            float inner = 1.0 - smoothstep(-fw, fw, innerDist);
            float ring  = clamp(outer - inner, 0.0, 1.0);
            color       = vec4(mix(vOutlineColor.rgb, vColor.rgb, inner),
                               max(vColor.a * inner, vOutlineColor.a * ring));
        }
        else
        {
            // Optional texture under the SDF mask (skinned rounded panels).
            vec4 texel = texture(uTextures[nonuniformEXT(vTextureIndex)], vUv);
            color      = vec4(texel.rgb * vColor.rgb, texel.a * vColor.a * outer);
        }
    }
    else
    {
        vec4 texel = texture(uTextures[nonuniformEXT(vTextureIndex)], vUv);
        color      = texel * vColor;
    }

    if (color.a < 0.001)
        discard;
    outColor = color;
}
