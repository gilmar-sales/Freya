#include "Freya/Asset/BakedAnimation.hpp"

#include <algorithm>
#include <cmath>

namespace FREYA_NAMESPACE
{
    namespace
    {
        float wrapTime(const float timeSec, const float duration,
                       const bool loop)
        {
            if (!loop || duration <= 0.f)
                return std::max(0.f, timeSec);
            float t = std::fmod(timeSec, duration);
            if (t < 0.f)
                t += duration;
            return t;
        }

        JointTRS lerpJoint(const JointTRS& a, const JointTRS& b, const float u)
        {
            JointTRS o;
            o.translation = glm::mix(a.translation, b.translation, u);
            o.rotation = glm::normalize(glm::slerp(a.rotation, b.rotation, u));
            o.scale    = glm::mix(a.scale, b.scale, u);
            return o;
        }
    } // namespace

    BakedClip BakeClip(const Skeleton& skeleton, const AnimationClip& clip,
                       float bakeHz)
    {
        bakeHz = std::max(bakeHz, 1.f);

        BakedClip baked;
        baked.duration   = std::max(clip.duration, 0.f);
        baked.jointCount = skeleton.JointCount();
        baked.frameDt    = 1.f / bakeHz;

        if (baked.jointCount == 0)
            return baked;

        if (baked.duration <= 1e-6f)
        {
            baked.frameCount = 1;
            const auto rest  = RestLocalPose(skeleton);
            baked.joints     = rest.joints;
            return baked;
        }

        const auto frames = static_cast<std::uint32_t>(
            std::max(2.0, std::ceil(static_cast<double>(baked.duration) *
                                    static_cast<double>(bakeHz))));
        baked.frameCount = frames;
        baked.joints.resize(
            static_cast<std::size_t>(frames) * baked.jointCount);

        for (std::uint32_t f = 0; f < frames; ++f)
        {
            const float t =
                (static_cast<float>(f) / static_cast<float>(frames)) *
                baked.duration;
            // Non-wrapping sample so the last bucket is not a duplicate of 0.
            const auto pose = SampleClip(skeleton, clip, t, false);
            for (std::uint32_t j = 0; j < baked.jointCount; ++j)
            {
                baked.joints[static_cast<std::size_t>(f) * baked.jointCount +
                             j] =
                    j < pose.Size() ? pose.joints[j] : JointTRS {};
            }
        }
        return baked;
    }

    LocalPose SampleBaked(const Skeleton& skeleton, const BakedClip& baked,
                          const float timeSec, const bool loop)
    {
        if (baked.Empty() || baked.jointCount != skeleton.JointCount())
            return RestLocalPose(skeleton);

        LocalPose pose;
        pose.Resize(baked.jointCount);

        if (baked.frameCount == 1)
        {
            for (std::uint32_t j = 0; j < baked.jointCount; ++j)
                pose.joints[j] = baked.joints[j];
            return pose;
        }

        const float t = wrapTime(timeSec, baked.duration, loop);
        float       u = 0.f;
        if (loop && baked.duration > 0.f)
        {
            u = (t / baked.duration) * static_cast<float>(baked.frameCount);
        }
        else if (baked.duration > 0.f)
        {
            u = (t / baked.duration) *
                static_cast<float>(baked.frameCount - 1u);
            u = std::clamp(u, 0.f, static_cast<float>(baked.frameCount - 1u));
        }

        auto        f0 = static_cast<std::uint32_t>(std::floor(u));
        const float a  = u - static_cast<float>(f0);
        if (loop)
            f0 %= baked.frameCount;
        else
            f0 = std::min(f0, baked.frameCount - 1u);

        const std::uint32_t f1 =
            loop ? (f0 + 1u) % baked.frameCount
                 : std::min(f0 + 1u, baked.frameCount - 1u);

        const auto base0 = static_cast<std::size_t>(f0) * baked.jointCount;
        const auto base1 = static_cast<std::size_t>(f1) * baked.jointCount;

        if (a <= 1e-6f || f0 == f1)
        {
            for (std::uint32_t j = 0; j < baked.jointCount; ++j)
                pose.joints[j] = baked.joints[base0 + j];
            return pose;
        }

        for (std::uint32_t j = 0; j < baked.jointCount; ++j)
        {
            pose.joints[j] =
                lerpJoint(baked.joints[base0 + j], baked.joints[base1 + j], a);
        }
        return pose;
    }

} // namespace FREYA_NAMESPACE
