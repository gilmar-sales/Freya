#version 450

struct UiInstance
{
    vec4 rect; // xywh pixels (top-left origin, Y down)
    vec4 uvRect;
    vec4 color;
    uint textureIndex;
    uint flags;
    float rounding;
    float borderWidth;
    float clipMax;
    float outlineWidth;
    float z;
    float _pad;
    vec4 outlineColor;
};

layout(std430, set = 0, binding = 0) readonly buffer InstanceBuffer
{
    UiInstance instances[];
};

layout(push_constant) uniform Push
{
    vec2 framebufferSize; // width, height
}
pc;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;
layout(location = 2) flat out uint vTextureIndex;
layout(location = 3) out float vClipU;
layout(location = 4) out float vClipMax;
layout(location = 5) flat out uint vFlags;
layout(location = 6) out vec4 vOutlineColor;
layout(location = 7) out float vOutlineWidth;
layout(location = 8) out vec2 vSizePx;
layout(location = 9) out float vRounding;
layout(location = 10) out float vBorderWidth;
layout(location = 11) out vec2 vUv01;

void main()
{
    // Two triangles covering [0,1]x[0,1] in local space (top-left origin).
    const vec2 corners[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0),
                                   vec2(0.0, 1.0), vec2(1.0, 0.0),
                                   vec2(1.0, 1.0), vec2(0.0, 1.0));
    const vec2 uvs[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
                               vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

    UiInstance inst = instances[gl_InstanceIndex];
    vec2       uv01 = uvs[gl_VertexIndex];
    vec2       local = corners[gl_VertexIndex];

    float x = inst.rect.x + local.x * inst.rect.z;
    float y = inst.rect.y + local.y * inst.rect.w;

    // Pixel (top-left, Y down) → Vulkan NDC (Y=-1 is top of viewport).
    float ndcX = (x / pc.framebufferSize.x) * 2.0 - 1.0;
    float ndcY = (y / pc.framebufferSize.y) * 2.0 - 1.0;

    gl_Position = vec4(ndcX, ndcY, inst.z, 1.0);

    vUv           = mix(inst.uvRect.xy, inst.uvRect.zw, uv01);
    vUv01         = uv01;
    vColor        = inst.color;
    vTextureIndex = inst.textureIndex;
    vClipU        = uv01.x;
    vClipMax      = inst.clipMax;
    vFlags        = inst.flags;
    vOutlineColor = inst.outlineColor;
    vOutlineWidth = inst.outlineWidth;
    vSizePx       = inst.rect.zw;
    vRounding     = inst.rounding;
    vBorderWidth  = inst.borderWidth;
}
