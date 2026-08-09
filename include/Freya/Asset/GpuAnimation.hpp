#pragma once

#include "Freya/Asset/BakedAnimation.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FREYA_NAMESPACE
{
    /** std430 mirror of Shader Anim/skin_bake.comp joints. */
    struct GpuBakedJoint
    {
        glm::vec3 t { 0.f };
        float     pad0 = 0.f;
        glm::vec4 q { 0.f, 0.f, 0.f, 1.f }; ///< xyzw
        glm::vec3 s { 1.f };
        float     pad1 = 0.f;
    };

    struct GpuClipHeader
    {
        float         duration   = 0.f;
        std::uint32_t frameCount = 0;
        std::uint32_t jointCount = 0;
        std::uint32_t jointsBase = 0; ///< index into joints[]
    };

    struct GpuAnimInstance
    {
        std::uint32_t boneOffset = 0;
        std::uint32_t jointCount = 0;
        std::uint32_t clipA      = 0;
        std::uint32_t clipB      = 0;
        float         timeA      = 0.f;
        float         timeB      = 0.f;
        float         blendT     = 0.f;
        std::uint32_t flags      = 1u; ///< bit0 = loop
    };

    [[nodiscard]] inline GpuBakedJoint ToGpuJoint(const JointTRS& j)
    {
        GpuBakedJoint g;
        g.t = j.translation;
        g.q = glm::vec4(j.rotation.x, j.rotation.y, j.rotation.z, j.rotation.w);
        g.s = j.scale;
        return g;
    }

    /**
     * @brief Pack clips into contiguous headers + joints for one SSBO.
     */
    struct GpuBakePack
    {
        std::vector<GpuClipHeader> headers;
        std::vector<GpuBakedJoint> joints;
    };

    [[nodiscard]] inline GpuBakePack PackBakedClips(
        std::span<const BakedClip> clips)
    {
        GpuBakePack pack;
        pack.headers.reserve(clips.size());
        for (const auto& c : clips)
        {
            GpuClipHeader h;
            h.duration   = c.duration;
            h.frameCount = c.frameCount;
            h.jointCount = c.jointCount;
            h.jointsBase = static_cast<std::uint32_t>(pack.joints.size());
            pack.headers.push_back(h);
            pack.joints.reserve(pack.joints.size() + c.joints.size());
            for (const auto& j : c.joints)
                pack.joints.push_back(ToGpuJoint(j));
        }
        return pack;
    }

    struct GpuSkeletonPack
    {
        std::uint32_t             jointCount = 0;
        std::vector<std::int32_t> parents;
        std::vector<glm::mat4>    inverseBind;
    };

    [[nodiscard]] inline GpuSkeletonPack PackSkeleton(const Skeleton& sk)
    {
        GpuSkeletonPack p;
        p.jointCount  = sk.JointCount();
        p.parents     = sk.parents;
        p.inverseBind = sk.inverseBind;
        if (p.parents.size() < p.jointCount)
            p.parents.resize(p.jointCount, -1);
        if (p.inverseBind.size() < p.jointCount)
            p.inverseBind.resize(p.jointCount, glm::mat4(1.f));
        return p;
    }

} // namespace FREYA_NAMESPACE
