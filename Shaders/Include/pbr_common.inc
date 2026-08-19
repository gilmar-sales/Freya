#ifndef PBR_COMMON_INC
#define PBR_COMMON_INC

const float PI = 3.14159265359;
const float kMinRoughness = 0.045;
const vec3 kDielectricF0 = vec3(0.04);

vec3 srgbToLinear(vec3 c) {
    return pow(c, vec3(2.2));
}

vec3 linearToSrgb(vec3 c) {
    return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
}

#endif
