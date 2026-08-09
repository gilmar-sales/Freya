#pragma once

#include "Freya/Asset/BakedAnimation.hpp"
#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Full-precision joint for clip/rest SSBOs (48 B, matches GLSL
     * BakedJoint when quantization is off).
     */
    struct GpuFloatJoint
    {
        glm::vec3 t { 0.f };
        float     pad0 = 0.f;
        glm::vec4 q { 0.f, 0.f, 0.f, 1.f }; ///< xyzw
        glm::vec3 s { 1.f };
        float     pad1 = 0.f;
    };

    static_assert(sizeof(GpuFloatJoint) == 48);

    /**
     * @brief Quantized joint for clip/rest SSBOs (16 B).
     *
     * Layout matches GLSL `QuantJoint` in Anim/skin_bake_body.inc:
     * - quatBits: smallest-three (2-bit omitted index + 3×10-bit)
     * - txy / tzsx / sysz: packHalf2x16 of (tx,ty), (tz,sx), (sy,sz)
     */
    struct GpuQuantJoint
    {
        std::uint32_t quatBits = 0;
        std::uint32_t txy      = 0;
        std::uint32_t tzsx     = 0;
        std::uint32_t sysz     = 0;
    };

    static_assert(sizeof(GpuQuantJoint) == 16);

    /**
     * @brief Working float TRS in compute scratch (matches GLSL BakedJoint).
     */
    struct GpuScratchJoint
    {
        glm::vec3 t { 0.f };
        float     pad0 = 0.f;
        glm::vec4 q { 0.f, 0.f, 0.f, 1.f };
        glm::vec3 s { 1.f };
        float     pad1 = 0.f;
    };

    static_assert(sizeof(GpuScratchJoint) == 48);

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
        constexpr std::uint32_t MaskedOverlay = 2u; ///< upper slot active
        constexpr std::uint32_t Additive      = 4u; ///< additive slot active
        constexpr std::uint32_t CancelRootXZ  = 8u;
    } // namespace GpuAnimFlags

    /**
     * @brief Per-actor GPU anim job (std430).
     *
     * Fixed AAA-style overlay contract (not a dynamic layer list):
     * loco Blend1D/2D → optional masked upper → optional additive → look / IK.
     * Weights ≤ 0 leave a slot unused. Shared look/IK joint indices come from
     * GpuAnimPass push constants.
     */
    struct GpuAnimInstance
    {
        std::uint32_t boneOffset = 0;
        std::uint32_t jointCount = 0;
        std::uint32_t clipA      = 0;
        std::uint32_t clipB      = 0;
        float         timeA      = 0.f;
        float         timeB      = 0.f;
        float         timeC      = 0.f;
        std::uint32_t flags      = GpuAnimFlags::Loop;
        /// Third loco sample (Blend2D). Unused when `wC == 0`.
        std::uint32_t clipC = 0;
        float         wA    = 1.f;
        float         wB    = 0.f;
        float         wC    = 0.f;
        /// Slot: OverrideMasked (upper-body, etc.)
        std::uint32_t clipMask   = 0;
        std::uint32_t maskBase   = 0; ///< index into boneMasks[]
        float         timeMask   = 0.f;
        float         weightMask = 0.f;
        /// Slot: Additive (idle breathe / recoil, …)
        std::uint32_t clipAdd   = 0;
        std::uint32_t _padLayer = 0;
        float         timeAdd   = 0.f;
        float         weightAdd = 0.f;
        glm::mat4     modelWorld { 1.f };
        glm::vec3     lookTarget { 0.f };
        float         lookWeight = 0.f;
        /// Per-instance look / two-bone chain (0xffffffff = disabled).
        std::uint32_t lookJoint = 0xffffffffu;
        std::uint32_t ikRoot    = 0xffffffffu;
        std::uint32_t ikMid     = 0xffffffffu;
        std::uint32_t ikTip     = 0xffffffffu;
        glm::vec3     lookLocalForward { 0.f, 0.f, 1.f };
        float         lookMaxYaw = 1.2f; ///< < 0 disables aim clamp
        glm::vec3     ikTarget { 0.f };
        float         ikWeight = 0.f;
        glm::vec3     ikPole { 0.f };
        float         lookMaxPitch = 0.8f;
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

    /// Pack unit quaternion: omit largest abs component (index in bits 31..30),
    /// store the other three as 10-bit values in [-1/sqrt(2), 1/sqrt(2)].
    [[nodiscard]] inline std::uint32_t PackQuatSmallestThree(glm::quat q)
    {
        q                = glm::normalize(q);
        const float c[4] = { q.x, q.y, q.z, q.w };
        int         maxI = 0;
        for (int i = 1; i < 4; ++i)
        {
            if (std::abs(c[i]) > std::abs(c[maxI]))
                maxI = i;
        }

        float v[4] = { c[0], c[1], c[2], c[3] };
        if (v[maxI] < 0.f)
        {
            for (float& e : v)
                e = -e;
        }

        constexpr float kRange = 0.7071067811865476f; // 1/sqrt(2)
        auto            out    = static_cast<std::uint32_t>(maxI) << 30;
        int             shift  = 20;
        for (int i = 0; i < 4; ++i)
        {
            if (i == maxI)
                continue;
            const float n =
                std::clamp((v[i] / kRange) * 0.5f + 0.5f, 0.f, 1.f);
            auto bits = static_cast<std::uint32_t>(n * 1023.f + 0.5f);
            bits      = std::min(bits, 1023u);
            out |= bits << shift;
            shift -= 10;
        }
        return out;
    }

    [[nodiscard]] inline GpuFloatJoint ToGpuFloatJoint(const JointTRS& j)
    {
        GpuFloatJoint g;
        g.t = j.translation;
        g.q = glm::vec4(j.rotation.x, j.rotation.y, j.rotation.z, j.rotation.w);
        g.s = j.scale;
        return g;
    }

    [[nodiscard]] inline GpuQuantJoint ToGpuQuantJoint(const JointTRS& j)
    {
        GpuQuantJoint g;
        g.quatBits = PackQuatSmallestThree(j.rotation);
        g.txy =
            glm::packHalf2x16(glm::vec2(j.translation.x, j.translation.y));
        g.tzsx = glm::packHalf2x16(glm::vec2(j.translation.z, j.scale.x));
        g.sysz = glm::packHalf2x16(glm::vec2(j.scale.y, j.scale.z));
        return g;
    }

    [[nodiscard]] inline std::uint64_t GpuClipKey(std::string_view name)
    {
        // FNV-1a 64 — stable across runs (std::hash is not).
        std::uint64_t h = 14695981039346656037ull;
        for (unsigned char c : name)
        {
            h ^= c;
            h *= 1099511628211ull;
        }
        return h == 0 ? 1ull : h;
    }

    [[nodiscard]] inline GpuClipHeader MakeGpuClipHeader(
        const BakedClip& clip, const std::uint32_t jointsBase)
    {
        GpuClipHeader h;
        h.duration   = clip.duration;
        h.frameCount = clip.frameCount;
        h.jointCount = clip.jointCount;
        h.jointsBase = jointsBase;
        return h;
    }

    [[nodiscard]] inline std::vector<GpuFloatJoint> PackClipJointsFloat(
        const BakedClip& clip)
    {
        std::vector<GpuFloatJoint> out;
        out.reserve(clip.joints.size());
        for (const auto& j : clip.joints)
            out.push_back(ToGpuFloatJoint(j));
        return out;
    }

    [[nodiscard]] inline std::vector<GpuQuantJoint> PackClipJointsQuant(
        const BakedClip& clip)
    {
        std::vector<GpuQuantJoint> out;
        out.reserve(clip.joints.size());
        for (const auto& j : clip.joints)
            out.push_back(ToGpuQuantJoint(j));
        return out;
    }

    /**
     * @brief Pack clips into contiguous headers + joints for one SSBO.
     */
    struct GpuBakePack
    {
        std::vector<GpuClipHeader> headers;
        std::vector<GpuFloatJoint> floatJoints;
        std::vector<GpuQuantJoint> quantJoints;
        bool                       quantized = false;

        [[nodiscard]] std::uint32_t JointCount() const
        {
            return quantized
                       ? static_cast<std::uint32_t>(quantJoints.size())
                       : static_cast<std::uint32_t>(floatJoints.size());
        }
    };

    [[nodiscard]] inline GpuBakePack PackBakedClips(
        std::span<const BakedClip> clips, const bool quantize = true)
    {
        GpuBakePack pack;
        pack.quantized = quantize;
        pack.headers.reserve(clips.size());
        for (const auto& c : clips)
        {
            GpuClipHeader h;
            h.duration   = c.duration;
            h.frameCount = c.frameCount;
            h.jointCount = c.jointCount;
            h.jointsBase =
                quantize ? static_cast<std::uint32_t>(pack.quantJoints.size())
                         : static_cast<std::uint32_t>(pack.floatJoints.size());
            pack.headers.push_back(h);
            if (quantize)
            {
                pack.quantJoints.reserve(pack.quantJoints.size() +
                                         c.joints.size());
                for (const auto& j : c.joints)
                    pack.quantJoints.push_back(ToGpuQuantJoint(j));
            }
            else
            {
                pack.floatJoints.reserve(pack.floatJoints.size() +
                                         c.joints.size());
                for (const auto& j : c.joints)
                    pack.floatJoints.push_back(ToGpuFloatJoint(j));
            }
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

    [[nodiscard]] inline std::vector<GpuFloatJoint> PackRestJointsFloat(
        const LocalPose& rest, const std::uint32_t jointCount)
    {
        std::vector<GpuFloatJoint> out(jointCount);
        const auto                 n = std::min(jointCount, rest.Size());
        for (std::uint32_t i = 0; i < n; ++i)
            out[i] = ToGpuFloatJoint(rest.joints[i]);
        for (std::uint32_t i = n; i < jointCount; ++i)
            out[i].q = glm::vec4(0.f, 0.f, 0.f, 1.f);
        return out;
    }

    [[nodiscard]] inline std::vector<GpuQuantJoint> PackRestJointsQuant(
        const LocalPose& rest, const std::uint32_t jointCount)
    {
        std::vector<GpuQuantJoint> out(jointCount);
        const auto                 n = std::min(jointCount, rest.Size());
        for (std::uint32_t i = 0; i < n; ++i)
            out[i] = ToGpuQuantJoint(rest.joints[i]);
        const JointTRS identity {};
        for (std::uint32_t i = n; i < jointCount; ++i)
            out[i] = ToGpuQuantJoint(identity);
        return out;
    }

} // namespace FREYA_NAMESPACE
