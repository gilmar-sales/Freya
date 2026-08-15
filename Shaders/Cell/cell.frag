#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inScene;
layout(set = 0, binding = 1) uniform sampler2D inDepth;
layout(set = 0, binding = 2) uniform sampler2D inNormal;

layout(set = 1, binding = 0) uniform sampler2D inAlbedo;
layout(std140, set = 1, binding = 1) uniform MaterialMask {
    uvec4 bits[2];
    uint count;
} mask;

layout(push_constant) uniform CellPush {
    float bands;
    float edgeDepthScale;
    float edgeNormalScale;
    float strength;
    vec4 edgeColor;
    float reverseZ;
    float shadowLift;
    float edgeWidth;
} push;

float SampleDepth(vec2 uv) {
    return texture(inDepth, uv).r;
}

vec3 SampleNormal(vec2 uv) {
    return normalize(texture(inNormal, uv).rgb * 2.0 - 1.0);
}

bool IsSky(float depth) {
    if (push.reverseZ > 0.5)
        return depth <= 1e-6;
    return depth >= 0.999999;
}

bool MaterialIncluded(uint matId) {
    if (mask.count == 0u)
        return true;
    uint word = mask.bits[matId >> 7u][(matId >> 5u) & 3u];
    return (word & (1u << (matId & 31u))) != 0u;
}

void main() {
    vec3 scene = texture(inScene, inUV).rgb;
    float depth = SampleDepth(inUV);
    uint matId = uint(texture(inAlbedo, inUV).a * 255.0 + 0.5);
    if (IsSky(depth) || !MaterialIncluded(matId)) {
        outColor = vec4(scene, 1.0);
        return;
    }

    float bands = max(push.bands, 1.0);
    float luma = dot(scene, vec3(0.2126, 0.7152, 0.0722));
    vec3 albedoLin = pow(max(texture(inAlbedo, inUV).rgb, vec3(0.0)), vec3(2.2));
    float albedoLuma =
        max(dot(albedoLin, vec3(0.2126, 0.7152, 0.0722)), 1e-4);
    vec3 chroma = (luma > 1e-4) ? scene / luma : albedoLin / albedoLuma;
    float lift = clamp(push.shadowLift, 0.0, 1.0);
    float quantized = max(floor(luma * bands + 0.5) / bands, lift);
    vec3 cel = chroma * quantized;

    ivec2 size = textureSize(inDepth, 0);
    vec2 texel = 1.0 / vec2(size);
    float widthPx =
        max(push.edgeWidth, 1.0) * max(float(size.y) / 1080.0, 1.0);
    vec2 step = texel * widthPx;

    float gx = SampleDepth(inUV + vec2(step.x, 0.0)) -
               SampleDepth(inUV - vec2(step.x, 0.0));
    float gy = SampleDepth(inUV + vec2(0.0, step.y)) -
               SampleDepth(inUV - vec2(0.0, step.y));
    float edgeDepth = length(vec2(gx, gy)) * push.edgeDepthScale;

    vec3 n0 = SampleNormal(inUV);
    vec3 nx = SampleNormal(inUV + vec2(step.x, 0.0));
    vec3 ny = SampleNormal(inUV - vec2(0.0, step.y));
    float edgeNormal =
        (1.0 - clamp(dot(n0, nx), 0.0, 1.0)) +
        (1.0 - clamp(dot(n0, ny), 0.0, 1.0));
    edgeNormal *= push.edgeNormalScale;

    float edge = clamp(max(edgeDepth, edgeNormal) * push.strength, 0.0, 1.0);
    outColor = vec4(mix(cel, push.edgeColor.rgb, edge), 1.0);
}
