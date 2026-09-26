#version 450

struct BillboardInstance
{
    vec3  worldPos;
    float clipMax;
    vec2  size;
    uint  textureIndex;
    uint  flags;
    vec4  color;
    vec4  uvRect;
    vec2  localOffset;
    float outlineWidth;
    float rotation; // screen-space rotation radians
    vec4  outlineColor;
    vec4  aux; // FixedAxis/Planar: (axisUp,0). VelocityStretch: (vel,scale).
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

layout(location = 0) out vec2  vUv;
layout(location = 1) out vec4  vColor;
layout(location = 2) flat out uint vTextureIndex;
layout(location = 3) out float vClipU;
layout(location = 4) out float vClipMax;
layout(location = 5) flat out uint vFlags;
layout(location = 6) out vec4  vOutlineColor;
layout(location = 7) out float vOutlineWidth;

const uint kAlignMask        = 7u;
const uint kAlignScreen      = 0u;
const uint kAlignCylindrical = 1u;
const uint kAlignSpherical   = 2u;
const uint kAlignFixedAxis   = 3u;
const uint kAlignPlanar      = 4u;
const uint kFlagScreenSize      = 32u;
const uint kFlagVelocityStretch = 64u;

void main()
{
    const vec2 corners[6] = vec2[](
        vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(-0.5,  0.5),
        vec2( 0.5, -0.5), vec2(0.5,  0.5), vec2(-0.5,  0.5));
    // Vulkan/stbi: (0,0) top-left. Bottom vertices get v=1.
    const vec2 uvs[6] = vec2[](
        vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(0.0, 0.0),
        vec2(1.0, 1.0), vec2(1.0, 0.0), vec2(0.0, 0.0));

    BillboardInstance inst   = instances[gl_InstanceIndex];
    vec2              corner = corners[gl_VertexIndex];

    // Camera basis vectors extracted from the view matrix.
    vec3 camRight = vec3(
        pc.view[0][0], pc.view[1][0], pc.view[2][0]);
    vec3 camUp = vec3(
        pc.view[0][1], pc.view[1][1], pc.view[2][1]);
    vec3 camFwd = vec3(
        -pc.view[0][2], -pc.view[1][2], -pc.view[2][2]);

    // Camera world position (extracted without matrix inverse):
    //   view[3] = (-dot(right,eye), -dot(up,eye), dot(fwd,eye), 1)
    //   eye = right*(-t.x) + up*(-t.y) + fwd*(t.z)
    vec3 camPos = camRight * (-pc.view[3][0])
                + camUp    * (-pc.view[3][1])
                + camFwd   *   pc.view[3][2];

    uint align = inst.flags & kAlignMask;

    if (align == kAlignCylindrical)
    {
        // Yaw-only: right = cross(camFwd, worldUp), up = worldUp.
        vec3 worldUp = vec3(0.0, 1.0, 0.0);
        camRight = cross(camFwd, worldUp);
        float len = length(camRight);
        camRight = len > 1e-6 ? camRight / len : vec3(1.0, 0.0, 0.0);
        camUp = worldUp;
    }
    else if (align == kAlignSpherical)
    {
        // Normal points toward the camera position per-instance.
        vec3 toCamera = normalize(camPos - inst.worldPos);
        vec3 ref = abs(toCamera.y) < 0.99
                       ? vec3(0.0, 1.0, 0.0)
                       : vec3(1.0, 0.0, 0.0);
        camRight = normalize(cross(ref, toCamera));
        camUp    = normalize(cross(toCamera, camRight));
    }
    else if (align == kAlignFixedAxis)
    {
        // Cylindrical with arbitrary up axis stored in aux.xyz.
        vec3 axisUp  = normalize(inst.aux.xyz);
        camRight = cross(camFwd, axisUp);
        float len = length(camRight);
        camRight = len > 1e-6 ? camRight / len : vec3(1.0, 0.0, 0.0);
        camUp = axisUp;
    }
    else if (align == kAlignPlanar)
    {
        // Flat on a surface; normal in aux.xyz; orientation is world-fixed.
        vec3 n   = normalize(inst.aux.xyz);
        vec3 ref = abs(n.z) < 0.99
                       ? vec3(0.0, 0.0, 1.0)
                       : vec3(0.0, 1.0, 0.0);
        camRight = normalize(cross(n, ref));
        camUp    = normalize(cross(camRight, n));
    }
    // else kAlignScreen: camRight/camUp stay as view-plane vectors.

    // Scale corners in local billboard space.
    vec2 scaled = corner * inst.size;

    // Constant screen-space size: compensate for perspective division.
    // inst.size is treated as NDC half-extents / projection scale.
    if ((inst.flags & kFlagScreenSize) != 0u)
    {
        vec4 clip = pc.proj * pc.view * vec4(inst.worldPos, 1.0);
        float w   = clip.w;
        scaled = vec2(corner.x * inst.size.x * w / pc.proj[0][0],
                      corner.y * inst.size.y * w / pc.proj[1][1]);
    }

    // Velocity stretch: long axis aligns with velocity; y size is
    // extended by length(velocity) * stretch scale.
    if ((inst.flags & kFlagVelocityStretch) != 0u)
    {
        vec3  vel       = inst.aux.xyz;
        float stretch   = inst.aux.w;
        float velLen    = length(vel);
        if (velLen > 1e-6)
        {
            vec3 velDir  = vel / velLen;
            camUp        = velDir;
            camRight     = normalize(cross(camUp, camFwd));
            if (length(camRight) < 1e-6)
                camRight = normalize(cross(camUp, vec3(1.0, 0.0, 0.0)));
            scaled.y += corner.y * velLen * stretch;
        }
    }

    // Screen-space rotation.
    if (abs(inst.rotation) > 1e-6)
    {
        float co = cos(inst.rotation);
        float si = sin(inst.rotation);
        scaled   = vec2(scaled.x * co - scaled.y * si,
                        scaled.x * si + scaled.y * co);
    }

    vec3 world = inst.worldPos
        + camRight * (inst.localOffset.x + scaled.x)
        + camUp    * (inst.localOffset.y + scaled.y);

    gl_Position   = pc.proj * pc.view * vec4(world, 1.0);
    vec2 uv01     = uvs[gl_VertexIndex];
    vUv           = mix(inst.uvRect.xy, inst.uvRect.zw, uv01);
    vColor        = inst.color;
    vTextureIndex = inst.textureIndex;
    vClipU        = uv01.x;
    vClipMax      = inst.clipMax;
    vFlags        = inst.flags;
    vOutlineColor = inst.outlineColor;
    vOutlineWidth = inst.outlineWidth;
}
