#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outVisibility;

layout(binding = 0) uniform sampler2D sceneDepth;
layout(binding = 1) uniform sampler2D sceneNormal;
layout(binding = 2) uniform sampler2DArrayShadow cascadeShadowMap;
// Kept bound for descriptor compatibility (PCSS removed).
layout(binding = 3) uniform sampler2DArray cascadeShadowDepth;

layout(binding = 4) uniform ShadowBuffer {
    mat4 cascadeViewProj[4];
    vec4 cascadeSplits;
    vec4 params;
    mat4 spotViewProj[4];
    vec4 spotLightIndex;
    vec4 pointLightPosFar[2];
    vec4 pointLightIndex;
    vec4 reverseZ;
    vec4 pcss;
} shadows;

layout(binding = 5) uniform CameraBuffer {
    mat4 invViewProj;
    vec4 viewPos;
    vec4 cameraForward;
    vec4 lightDirection;
} camera;

const vec2 kPoissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790));

// Same fixed soft disk as forward/spot/point (no IGN rotation, no PCSS).
const float kSoftShadowTexels = 5.0;

float SoftShadowPCF(vec2 uv, float layer, float depthRef) {
    vec2 texel = 1.0 / vec2(textureSize(cascadeShadowMap, 0).xy);
    float result = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec2 offset = kPoissonDisk[i] * kSoftShadowTexels * texel;
        result +=
            texture(cascadeShadowMap, vec4(uv + offset, layer, depthRef));
    }
    return result / 16.0;
}

float SoftCascadeShadow(mat4 lightVP, vec3 worldPos, vec3 N, vec3 L,
                        float layer) {
    vec3 lightDir = normalize(L);
    float nDotL = max(dot(N, lightDir), 0.0);
    float slope = sqrt(max(1.0 - nDotL * nDotL, 0.0));
    bool reverseZ = shadows.reverseZ.x > 0.5;

    float normalScale = shadows.params.y * (0.35 + 0.65 * slope);
    float lightPush = shadows.params.x * (0.5 + slope);
    vec3 biased = worldPos + N * normalScale + lightDir * lightPush;
    vec4 clip = lightVP * vec4(biased, 1.0);
    vec3 ndc = clip.xyz / clip.w;

    if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0)
        return 1.0;

    vec2 uv = ndc.xy * 0.5 + 0.5;
    float depthBias = shadows.params.x * (1.0 + 1.5 * slope);
    float depthRef = reverseZ ? (ndc.z + depthBias) : (ndc.z - depthBias);
    return SoftShadowPCF(uv, layer, depthRef);
}

vec3 ReconstructWorldPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 worldH = camera.invViewProj * clip;
    return worldH.xyz / worldH.w;
}

void main() {
    int cascadeCount = int(shadows.params.z);
    if (cascadeCount < 1) {
        outVisibility = 1.0;
        return;
    }

    float depth = texture(sceneDepth, inUV).r;
    bool reverseZ = shadows.reverseZ.x > 0.5;
    bool isSky = reverseZ ? (depth <= 0.0001) : (depth >= 0.9999);
    if (isSky) {
        outVisibility = 1.0;
        return;
    }

    vec3 worldPos = ReconstructWorldPos(inUV, depth);
    vec3 N = normalize(texture(sceneNormal, inUV).xyz);
    vec3 L = -normalize(camera.lightDirection.xyz);

    float viewDepth =
        dot(worldPos - camera.viewPos.xyz, camera.cameraForward.xyz);

    int cascade = cascadeCount - 1;
    for (int i = 0; i < cascadeCount; ++i) {
        if (viewDepth < shadows.cascadeSplits[i]) {
            cascade = i;
            break;
        }
    }

    float shadow = SoftCascadeShadow(shadows.cascadeViewProj[cascade],
                                     worldPos, N, L, float(cascade));

    if (cascade < cascadeCount - 1) {
        float splitNear =
            (cascade == 0) ? 0.0 : shadows.cascadeSplits[cascade - 1];
        float splitFar = shadows.cascadeSplits[cascade];
        float blendStart = mix(splitNear, splitFar, 0.9);
        if (viewDepth > blendStart) {
            float next =
                SoftCascadeShadow(shadows.cascadeViewProj[cascade + 1],
                                  worldPos, N, L, float(cascade + 1));
            float t = smoothstep(blendStart, splitFar, viewDepth);
            shadow = mix(shadow, next, t);
        }
    }

    outVisibility = shadow;
}
