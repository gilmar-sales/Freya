#include "Freya/Asset/Pose.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace FREYA_NAMESPACE
{
    namespace
    {
        template <typename KeyT, typename ValueT>
        ValueT sampleKeys(const std::vector<KeyT>& keys, const float time,
                          ValueT fallback,
                          ValueT (*lerpFn)(const ValueT&, const ValueT&, float))
        {
            if (keys.empty())
                return fallback;
            if (keys.size() == 1 || time <= keys.front().time)
                return keys.front().value;
            if (time >= keys.back().time)
                return keys.back().value;

            std::size_t next = 1;
            while (next < keys.size() && time > keys[next].time)
                ++next;
            const std::size_t prev = next - 1;
            const float       t0   = keys[prev].time;
            const float       t1   = keys[next].time;
            const float       u =
                (t1 > t0) ? std::clamp((time - t0) / (t1 - t0), 0.f, 1.f) : 0.f;
            return lerpFn(keys[prev].value, keys[next].value, u);
        }

        glm::vec3 lerpVec(const glm::vec3& a, const glm::vec3& b, float u)
        {
            return glm::mix(a, b, u);
        }

        glm::quat lerpQuat(const glm::quat& a, const glm::quat& b, float u)
        {
            return glm::normalize(glm::slerp(a, b, u));
        }

        JointTRS decomposeMatrix(const glm::mat4& m)
        {
            JointTRS j;
            j.translation = glm::vec3(m[3]);
            j.scale       = glm::vec3(glm::length(glm::vec3(m[0])),
                                      glm::length(glm::vec3(m[1])),
                                      glm::length(glm::vec3(m[2])));
            const glm::mat3 rotMat(
                glm::vec3(m[0]) / std::max(j.scale.x, 1e-8f),
                glm::vec3(m[1]) / std::max(j.scale.y, 1e-8f),
                glm::vec3(m[2]) / std::max(j.scale.z, 1e-8f));
            j.rotation = glm::normalize(glm::quat_cast(rotMat));
            return j;
        }

        float wrapTime(float timeSec, float duration, bool loop)
        {
            if (!loop || duration <= 0.f)
                return std::max(0.f, timeSec);
            float t = std::fmod(timeSec, duration);
            if (t < 0.f)
                t += duration;
            return t;
        }

        std::int32_t findRootJoint(const Skeleton& skeleton)
        {
            for (std::uint32_t i = 0; i < skeleton.JointCount(); ++i)
            {
                if (skeleton.parents[i] < 0)
                    return static_cast<std::int32_t>(i);
            }
            return skeleton.JointCount() > 0 ? 0 : -1;
        }
    } // namespace

    glm::mat4 JointTRS::ToMatrix() const
    {
        return glm::translate(glm::mat4(1.f), translation) *
               glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.f), scale);
    }

    void LocalPose::Resize(const std::uint32_t jointCount)
    {
        joints.assign(jointCount, JointTRS {});
    }

    LocalPose RestLocalPose(const Skeleton& skeleton)
    {
        LocalPose  pose;
        const auto n = skeleton.JointCount();
        pose.Resize(n);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            if (i < skeleton.restLocal.size())
                pose.joints[i] = decomposeMatrix(skeleton.restLocal[i]);
        }
        return pose;
    }

    LocalPose SampleClip(const Skeleton& skeleton, const AnimationClip& clip,
                         const float timeSec, const bool loop)
    {
        LocalPose  pose       = RestLocalPose(skeleton);
        const auto jointCount = skeleton.JointCount();
        if (jointCount == 0)
            return pose;

        const float time = wrapTime(timeSec, clip.duration, loop);

        for (const auto& ch : clip.channels)
        {
            if (ch.jointIndex >= jointCount)
                continue;

            JointTRS& j = pose.joints[ch.jointIndex];
            if (!ch.translations.empty())
                j.translation =
                    sampleKeys(ch.translations, time, j.translation, lerpVec);
            if (!ch.rotations.empty())
                j.rotation =
                    sampleKeys(ch.rotations, time, j.rotation, lerpQuat);
            if (!ch.scales.empty())
                j.scale = sampleKeys(ch.scales, time, j.scale, lerpVec);
        }
        return pose;
    }

    LocalPose BlendLocalPoses(const LocalPose& a, const LocalPose& b,
                              const float t)
    {
        const float u = std::clamp(t, 0.f, 1.f);
        if (u <= 0.f)
            return a;
        if (u >= 1.f)
            return b;

        LocalPose  out;
        const auto n = std::min(a.Size(), b.Size());
        out.Resize(n);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            out.joints[i].translation =
                glm::mix(a.joints[i].translation, b.joints[i].translation, u);
            out.joints[i].rotation = glm::normalize(
                glm::slerp(a.joints[i].rotation, b.joints[i].rotation, u));
            out.joints[i].scale =
                glm::mix(a.joints[i].scale, b.joints[i].scale, u);
        }
        return out;
    }

    std::vector<glm::mat4> LocalToGlobal(const Skeleton&  skeleton,
                                         const LocalPose& local)
    {
        const auto             n = skeleton.JointCount();
        std::vector<glm::mat4> global(n, glm::mat4(1.f));
        for (std::uint32_t i = 0; i < n; ++i)
        {
            const glm::mat4 localM =
                i < local.Size() ? local.joints[i].ToMatrix() : glm::mat4(1.f);
            const auto parent = i < skeleton.parents.size()
                                    ? skeleton.parents[i]
                                    : std::int32_t { -1 };
            if (parent >= 0 && static_cast<std::uint32_t>(parent) < n)
                global[i] = global[static_cast<std::uint32_t>(parent)] * localM;
            else
                global[i] = localM;
        }
        return global;
    }

    std::vector<glm::mat4> PoseToSkinMatrices(const Skeleton&  skeleton,
                                              const LocalPose& local)
    {
        auto       global = LocalToGlobal(skeleton, local);
        const auto n      = skeleton.JointCount();
        for (std::uint32_t i = 0; i < n; ++i)
        {
            const glm::mat4 ib = i < skeleton.inverseBind.size()
                                     ? skeleton.inverseBind[i]
                                     : glm::mat4(1.f);
            global[i]          = global[i] * ib;
        }
        return global;
    }

    RootMotionDelta ExtractRootDelta(const Skeleton&      skeleton,
                                     const AnimationClip& clip, const float t0,
                                     const float t1, const bool loop)
    {
        RootMotionDelta delta {};
        const auto      root = findRootJoint(skeleton);
        if (root < 0)
            return delta;

        const auto ri = static_cast<std::uint32_t>(root);
        auto       a  = SampleClip(skeleton, clip, t0, loop);
        auto       b  = SampleClip(skeleton, clip, t1, loop);
        if (ri >= a.Size() || ri >= b.Size())
            return delta;

        // Loop unwrap: if t1 wrapped below t0, accumulate end→duration end
        // then 0→t1 via a second sample at duration when needed.
        if (loop && clip.duration > 0.f && t1 < t0)
        {
            auto endPose =
                SampleClip(skeleton, clip, clip.duration - 1e-4f, false);
            const glm::vec3 d0 =
                endPose.joints[ri].translation - a.joints[ri].translation;
            const glm::vec3 d1 =
                b.joints[ri].translation -
                SampleClip(skeleton, clip, 0.f, false).joints[ri].translation;
            delta.translation = d0 + d1;
            delta.rotation    = glm::normalize(
                b.joints[ri].rotation * glm::inverse(a.joints[ri].rotation));
            return delta;
        }

        delta.translation = b.joints[ri].translation - a.joints[ri].translation;
        delta.rotation    = glm::normalize(
            b.joints[ri].rotation * glm::inverse(a.joints[ri].rotation));
        return delta;
    }

} // namespace FREYA_NAMESPACE
