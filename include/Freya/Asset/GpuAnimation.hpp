#pragma once

#include "Freya/Asset/BakedAnimation.hpp"
#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <algorithm>
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

    namespace GpuAnimFlags
    {
        constexpr std::uint32_t Loop          = 1u;
        constexpr std::uint32_t MaskedOverlay = 2u;
        constexpr std::uint32_t Additive      = 4u;
        constexpr std::uint32_t CancelRootXZ  = 8u;
    } // namespace GpuAnimFlags

    /**
     * @brief Per-actor GPU anim job (std430, 160 bytes).
     *
     * Loco Blend1D + optional mask/additive layer, then optional look-at /
     * two-bone IK using `modelWorld` and world-space targets. Weights ≤0
     * disable look/IK. Shared joint indices come from GpuAnimPass push
     * constants.
     */
    struct GpuAnimInstance
    {
        std::uint32_t boneOffset  = 0;
        std::uint32_t jointCount  = 0;
        std::uint32_t clipA       = 0;
        std::uint32_t clipB       = 0;
        float         timeA       = 0.f;
        float         timeB       = 0.f;
        float         blendT      = 0.f;
        std::uint32_t flags       = GpuAnimFlags::Loop;
        std::uint32_t clipLayer   = 0;
        std::uint32_t maskBase    = 0; ///< index into boneMasks[]
        float         timeLayer   = 0.f;
        float         layerWeight = 0.f;
        glm::mat4     modelWorld { 1.f };
        glm::vec3     lookTarget { 0.f };
        float         lookWeight = 0.f;
        glm::vec3     ikTarget { 0.f };
        float         ikWeight = 0.f;
        glm::vec3     ikPole { 0.f };
        float         _pad0 = 0.f;
    };

    /**
     * @brief One bone to pull from the GPU palette for async CPU use (N+1).
     *
     * `boneOffset + jointIndex` indexes `BoneMatrixResources::bones[]`
     * (skin matrix = global * inverseBind). Prefer a few hero joints over
     * full-palette readback.
     */
    struct GpuJointExtractRequest
    {
        std::uint32_t boneOffset = 0;
        std::uint32_t jointIndex = 0;
    };

    /**
     * @brief Result of #GpuAnimPass::PollJointExtract (previous finished
     * frame).
     */
    struct GpuJointExtractSample
    {
        std::uint32_t boneOffset  = 0;
        std::uint32_t jointIndex  = 0;
        std::uint32_t sourceFrame = 0;
        std::uint32_t _pad        = 0;
        glm::mat4     skinMatrix { 1.f };
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

    [[nodiscard]] inline std::vector<float> PackBoneMask(
        const BoneMask& mask, std::uint32_t jointCount)
    {
        std::vector<float> out(jointCount, 0.f);
        const auto         n = std::min(jointCount, mask.Size());
        for (std::uint32_t i = 0; i < n; ++i)
            out[i] = std::clamp(mask.weights[i], 0.f, 1.f);
        return out;
    }

    [[nodiscard]] inline std::vector<GpuBakedJoint> PackRestJoints(
        const LocalPose& rest, const std::uint32_t jointCount)
    {
        std::vector<GpuBakedJoint> out(jointCount);
        const auto                 n = std::min(jointCount, rest.Size());
        for (std::uint32_t i = 0; i < n; ++i)
            out[i] = ToGpuJoint(rest.joints[i]);
        for (std::uint32_t i = n; i < jointCount; ++i)
            out[i].q = glm::vec4(0.f, 0.f, 0.f, 1.f);
        return out;
    }

} // namespace FREYA_NAMESPACE
