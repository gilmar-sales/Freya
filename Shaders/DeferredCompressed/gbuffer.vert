#version 450

layout(binding = 0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj; // jittered
    vec4 ambientLight;
    // Match C++ alignas(64) padding before invViewProjection.
    vec4 _pad0;
    vec4 _pad1;
    vec4 _pad2;
    mat4 invViewProjection;
    mat4 prevViewProjection;
    mat4 unjitteredProjection;
} pub;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec2 inTexCoord;
layout (location = 5) in mat4 inModel;
layout (location = 9) in mat4 inPrevModel;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec2 outTexCoord;
layout (location = 2) out mat3 outTBN;
layout (location = 5) out vec3 outColor;
layout (location = 6) out vec2 outVelocity;

void main() {
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    vec4 prevWorldPos = inPrevModel * vec4(inPosition, 1.0);
    gl_Position = pub.proj * pub.view * worldPos;
    outPosition = worldPos.xyz;
    outTexCoord = inTexCoord;
    outColor = inColor;

    // Motion in UV space: unjittered current VP vs previous VP, each with
    // the matching model matrix (prevModel for dynamic objects / TAA).
    vec4 currClip = pub.unjitteredProjection * pub.view * worldPos;
    vec4 prevClip = pub.prevViewProjection * prevWorldPos;
    vec2 currNdc = currClip.xy / max(currClip.w, 1e-5);
    vec2 prevNdc = prevClip.xy / max(prevClip.w, 1e-5);
    outVelocity = (currNdc - prevNdc) * 0.5;

    vec3 T = normalize(vec3(inModel * vec4(inTangent, 0.0)));
    vec3 N = normalize(vec3(inModel * vec4(inNormal, 0.0)));
    T = normalize(T - N * dot(N, T));
    vec3 B = cross(N, T);

    outTBN = mat3(T, B, N);
}
