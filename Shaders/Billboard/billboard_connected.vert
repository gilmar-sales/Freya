#version 450

// Explicit-quad strip segments (see ConnectedBillboardGpuInstance).
// Unlike billboard.vert, corners are world positions computed on the
// CPU with miter joints, so consecutive instances share edge vertices
// exactly and connect without gaps or overlaps.
struct ConnectedInstance
{
    vec4  c0;
    vec4  c1;
    vec4  c2;
    vec4  c3;
    vec4  color0;
    vec4  color1;
    vec4  uvRect;
    uint  textureIndex;
    uint  flags;
    float clipMax;
    float pad;
};

layout(std430, set = 0, binding = 1) readonly buffer ConnectedBuffer
{
    ConnectedInstance instances[];
};

layout(push_constant) uniform Push
{
    mat4 view;
    mat4 proj;
}
pc;

layout(location = 0) out vec2  vUv;
layout(location = 1) out vec4  vColor;
layout(location = 2) flat out uint vTextureIndex;
layout(location = 3) out float vClipU;
layout(location = 4) out float vClipMax;
layout(location = 5) flat out uint vFlags;
layout(location = 6) out vec4  vOutlineColor;
layout(location = 7) out float vOutlineWidth;

void main()
{
    // Triangles (c0, c1, c2) and (c1, c3, c2).
    ConnectedInstance inst = instances[gl_InstanceIndex];
    vec3  pos;
    float t;
    vec2  uv01;
    if (gl_VertexIndex == 0)
    {
        pos = inst.c0.xyz; t = 0.0; uv01 = vec2(0.0, 0.0);
    }
    else if (gl_VertexIndex == 1)
    {
        pos = inst.c1.xyz; t = 0.0; uv01 = vec2(0.0, 1.0);
    }
    else if (gl_VertexIndex == 2)
    {
        pos = inst.c2.xyz; t = 1.0; uv01 = vec2(1.0, 0.0);
    }
    else if (gl_VertexIndex == 3)
    {
        pos = inst.c1.xyz; t = 0.0; uv01 = vec2(0.0, 1.0);
    }
    else if (gl_VertexIndex == 4)
    {
        pos = inst.c3.xyz; t = 1.0; uv01 = vec2(1.0, 1.0);
    }
    else
    {
        pos = inst.c2.xyz; t = 1.0; uv01 = vec2(1.0, 0.0);
    }

    gl_Position   = pc.proj * pc.view * vec4(pos, 1.0);
    vUv           = mix(inst.uvRect.xy, inst.uvRect.zw, uv01);
    vColor        = mix(inst.color0, inst.color1, t);
    vTextureIndex = inst.textureIndex;
    vClipU        = mix(inst.uvRect.x, inst.uvRect.z, uv01.x);
    vClipMax      = inst.clipMax;
    vFlags        = inst.flags;
    vOutlineColor = vec4(0.0, 0.0, 0.0, 1.0);
    vOutlineWidth = 0.0;
}
