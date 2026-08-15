#version 450

struct BillboardInstance
{
    vec3 worldPos;
    float clipMax;
    vec2 size;
    uint textureIndex;
    uint flags;
    vec4 color;
    vec4 uvRect;
    vec2 localOffset;
    vec2 _pad;
};

layout(std430, set = 0, binding = 0) readonly buffer InstanceBuffer
{
    BillboardInstance instances[];
};

layout(push_constant) uniform Push
{
    mat4 view;
    mat4 proj;
}
pc;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;
layout(location = 2) flat out uint vTextureIndex;
layout(location = 3) out float vClipU;
layout(location = 4) out float vClipMax;
layout(location = 5) flat out uint vFlags;

const uint kCylindrical = 1u;

void main()
{
    const vec2 corners[6] = vec2[](vec2(-0.5, -0.5), vec2(0.5, -0.5),
                                   vec2(-0.5, 0.5), vec2(0.5, -0.5),
                                   vec2(0.5, 0.5), vec2(-0.5, 0.5));
    // Vulkan/stbi: (0,0) top-left. Bottom vertices get v = 1.
    const vec2 uvs[6] = vec2[](vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(0.0, 0.0),
                               vec2(1.0, 1.0), vec2(1.0, 0.0), vec2(0.0, 0.0));

    BillboardInstance inst = instances[gl_InstanceIndex];
    vec2              corner = corners[gl_VertexIndex];

    vec3 camRight = vec3(pc.view[0][0], pc.view[1][0], pc.view[2][0]);
    vec3 camUp    = vec3(pc.view[0][1], pc.view[1][1], pc.view[2][1]);
    if ((inst.flags & kCylindrical) != 0u)
    {
        vec3 worldUp = vec3(0.0, 1.0, 0.0);
        vec3 fwd = vec3(-pc.view[0][2], -pc.view[1][2], -pc.view[2][2]);
        // Match glm::lookAt: right = cross(forward, up).
        camRight = cross(fwd, worldUp);
        float len = length(camRight);
        camRight = len > 1e-6 ? camRight / len : vec3(1.0, 0.0, 0.0);
        camUp    = worldUp;
    }

    vec3 world = inst.worldPos
        + camRight * (inst.localOffset.x + corner.x * inst.size.x)
        + camUp * (inst.localOffset.y + corner.y * inst.size.y);

    gl_Position   = pc.proj * pc.view * vec4(world, 1.0);
    vec2 uv01     = uvs[gl_VertexIndex];
    vUv           = mix(inst.uvRect.xy, inst.uvRect.zw, uv01);
    vColor        = inst.color;
    vTextureIndex = inst.textureIndex;
    vClipU        = uv01.x;
    vClipMax      = inst.clipMax;
    vFlags        = inst.flags;
}
