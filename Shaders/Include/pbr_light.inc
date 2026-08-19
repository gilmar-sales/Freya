#ifndef PBR_LIGHT_INC
#define PBR_LIGHT_INC

// Point / directional / spot / area-as-point. Area rects use LTC instead
// (see ltc.inc); this fallback is for OIT and any path without LTC LUTs.
void resolveLight(vec3 lightPos, float lightType, float radius, vec3 lightDir,
                  float innerCutoff, float outerCutoff, vec3 worldPos,
                  out vec3 L, out float attenuation) {
    L = vec3(0.0);
    attenuation = 1.0;

    if (lightType < 0.5) {
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        L = normalize(toLight);
        attenuation = radius / (dist * dist + 1.0);
    } else if (lightType < 1.5) {
        L = -normalize(lightDir);
        attenuation = 1.0;
    } else if (lightType < 2.5) {
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        L = normalize(toLight);

        float spotCos = dot(L, -normalize(lightDir));
        float spotFactor = smoothstep(outerCutoff, innerCutoff, spotCos);
        attenuation = spotFactor * radius / (dist * dist + 1.0);
    } else {
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        L = normalize(toLight);
        attenuation = radius / (dist * dist + 1.0);
    }
}

#endif
