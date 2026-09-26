#include "Freya/Asset/GpuAnimation.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/packing.hpp>

namespace FREYA_NAMESPACE
{
    std::uint32_t PackQuatSmallestThree(glm::quat q)
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
            const float n = std::clamp((v[i] / kRange) * 0.5f + 0.5f, 0.f, 1.f);
            auto        bits = static_cast<std::uint32_t>(n * 1023.f + 0.5f);
            bits             = std::min(bits, 1023u);
            out |= bits << shift;
            shift -= 10;
        }
        return out;
    }

    GpuFloatJoint ToGpuFloatJoint(const JointTRS& j)
    {
        GpuFloatJoint g;
        g.t = j.translation;
        g.q = glm::vec4(j.rotation.x, j.rotation.y, j.rotation.z, j.rotation.w);
        g.s = j.scale;
        return g;
    }

    GpuQuantJoint ToGpuQuantJoint(const JointTRS& j)
    {
        GpuQuantJoint g;
        g.quatBits = PackQuatSmallestThree(j.rotation);
        g.txy  = glm::packHalf2x16(glm::vec2(j.translation.x, j.translation.y));
        g.tzsx = glm::packHalf2x16(glm::vec2(j.translation.z, j.scale.x));
        g.sysz = glm::packHalf2x16(glm::vec2(j.scale.y, j.scale.z));
        return g;
    }

    std::uint64_t GpuClipKey(std::string_view name)
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

    GpuClipHeader MakeGpuClipHeader(const BakedClip& clip,
                                    std::uint32_t jointsBase)
    {
        GpuClipHeader h;
        h.duration   = clip.duration;
        h.frameCount = clip.frameCount;
        h.jointCount = clip.jointCount;
        h.jointsBase = jointsBase;
        return h;
    }

    std::vector<GpuFloatJoint> PackClipJointsFloat(const BakedClip& clip)
    {
        std::vector<GpuFloatJoint> out;
        out.reserve(clip.joints.size());
        for (const auto& j : clip.joints)
            out.push_back(ToGpuFloatJoint(j));
        return out;
    }

    std::vector<GpuQuantJoint> PackClipJointsQuant(const BakedClip& clip)
    {
        std::vector<GpuQuantJoint> out;
        out.reserve(clip.joints.size());
        for (const auto& j : clip.joints)
            out.push_back(ToGpuQuantJoint(j));
        return out;
    }

    std::uint32_t GpuBakePack::JointCount() const
    {
        return quantized ? static_cast<std::uint32_t>(quantJoints.size())
                         : static_cast<std::uint32_t>(floatJoints.size());
    }

    GpuBakePack PackBakedClips(std::span<const BakedClip> clips, bool quantize)
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

    std::uint64_t GpuSkeletonKey(std::string_view name)
    {
        return GpuClipKey(name);
    }

    GpuSkeletonPack PackSkeleton(const Skeleton& sk)
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

    std::vector<float> PackBoneMask(const BoneMask& mask,
                                    std::uint32_t jointCount)
    {
        std::vector<float> out(jointCount, 0.f);
        const auto         n = std::min(jointCount, mask.Size());
        for (std::uint32_t i = 0; i < n; ++i)
            out[i] = std::clamp(mask.weights[i], 0.f, 1.f);
        return out;
    }

    std::vector<GpuFloatJoint> PackRestJointsFloat(const LocalPose& rest,
                                                   std::uint32_t jointCount)
    {
        std::vector<GpuFloatJoint> out(jointCount);
        const auto                 n = std::min(jointCount, rest.Size());
        for (std::uint32_t i = 0; i < n; ++i)
            out[i] = ToGpuFloatJoint(rest.joints[i]);
        for (std::uint32_t i = n; i < jointCount; ++i)
            out[i].q = glm::vec4(0.f, 0.f, 0.f, 1.f);
        return out;
    }

    std::vector<GpuQuantJoint> PackRestJointsQuant(const LocalPose& rest,
                                                   std::uint32_t jointCount)
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
