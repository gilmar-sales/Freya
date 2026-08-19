#ifndef LTC_INC
#define LTC_INC

#include "pbr_common.inc"

// Requires sampler2D ltcMatrixMap and ltcAmplMap.
const float LUT_SIZE = 64.0;
const float LUT_SCALE = (LUT_SIZE - 1.0) / LUT_SIZE;
const float LUT_BIAS = 0.5 / LUT_SIZE;

vec3 IntegrateEdgeVec(vec3 v1, vec3 v2) {
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta =
        (x > 0.0) ? v
                  : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

    return cross(v1, v2) * theta_sintheta;
}

float LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4],
                   bool twoSided) {
    vec3 T1 = normalize(V - N * dot(V, N));
    vec3 T2 = cross(N, T1);
    Minv = Minv * transpose(mat3(T1, T2, N));

    vec3 L[4];
    L[0] = Minv * (points[0] - P);
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);

    vec3 dir = points[0] - P;
    vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);

    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    vec3 vsum = IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    float len = length(vsum);
    float z = vsum.z / max(len, 1e-6);
    if (behind)
        z = -z;

    vec2 uv = vec2(z * 0.5 + 0.5, len);
    uv = uv * LUT_SCALE + LUT_BIAS;

    float scale = texture(ltcAmplMap, uv).w;
    float sum = len * scale;
    if (!behind && !twoSided)
        sum = 0.0;

    return max(sum, 0.0);
}

vec3 calculateAreaLight(vec3 center, vec3 lightColor, vec3 normal,
                        vec3 tangent, float halfWidth, float halfHeight,
                        float intensity, vec3 worldPos, vec3 N, vec3 V,
                        vec3 albedo, float roughness, float metalness,
                        vec3 F0, float coatSpecAtten) {
    vec3 Nn = normalize(normal);
    vec3 T = normalize(tangent - Nn * dot(tangent, Nn));
    vec3 B = cross(Nn, T);

    vec3 points[4];
    points[0] = center - T * halfWidth - B * halfHeight;
    points[1] = center + T * halfWidth - B * halfHeight;
    points[2] = center + T * halfWidth + B * halfHeight;
    points[3] = center - T * halfWidth + B * halfHeight;

    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    vec2 uv = vec2(roughness, sqrt(1.0 - NdotV));
    uv = uv * LUT_SCALE + LUT_BIAS;

    vec4 t1 = texture(ltcMatrixMap, uv);
    vec4 t2 = texture(ltcAmplMap, uv);

    mat3 Minv = mat3(vec3(t1.x, 0.0, t1.y),
                     vec3(0.0, 1.0, 0.0),
                     vec3(t1.z, 0.0, t1.w));

    float spec = LTC_Evaluate(N, V, worldPos, Minv, points, true);
    float diff =
        LTC_Evaluate(N, V, worldPos, mat3(1.0), points, true);

    vec3 specular = F0 * t2.x + (1.0 - F0) * t2.y;
    specular *= spec * coatSpecAtten;
    vec3 diffuse = albedo * (1.0 - metalness) * diff;

    return lightColor * intensity * (diffuse + specular) / (2.0 * PI);
}

#endif
