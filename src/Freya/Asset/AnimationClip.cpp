#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace FREYA_NAMESPACE
{
    namespace
    {
        template <typename KeyT, typename ValueT>
        ValueT sampleKeys(const std::vector<KeyT>& keys, const float time,
                          ValueT                      fallback,
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

        glm::mat4 composeTrs(const glm::vec3& t, const glm::quat& r,
                             const glm::vec3& s)
        {
            return glm::translate(glm::mat4(1.f), t) * glm::mat4_cast(r) *
                   glm::scale(glm::mat4(1.f), s);
        }
    } // namespace

    std::vector<glm::mat4> EvaluateSkeletonPose(const Skeleton&      skeleton,
                                                const AnimationClip& clip,
                                                float                timeSec)
    {
        const auto jointCount = skeleton.JointCount();
        std::vector<glm::mat4> local = skeleton.restLocal;
        if (local.size() != jointCount)
            local.assign(jointCount, glm::mat4(1.f));

        float duration = clip.duration;
        if (duration <= 0.f && clip.ticksPerSecond > 0.f)
            duration = 0.f;
        float time = timeSec;
        if (duration > 0.f)
        {
            time = std::fmod(timeSec, duration);
            if (time < 0.f)
                time += duration;
        }

        // Clip times are stored in seconds.
        for (const auto& ch : clip.channels)
        {
            if (ch.jointIndex >= jointCount)
                continue;

            glm::vec3 T(0.f);
            glm::quat R(1.f, 0.f, 0.f, 0.f);
            glm::vec3 S(1.f);

            // Decompose rest as fallback when a track is missing.
            {
                const glm::mat4& rest = local[ch.jointIndex];
                T = glm::vec3(rest[3]);
                S = glm::vec3(glm::length(glm::vec3(rest[0])),
                              glm::length(glm::vec3(rest[1])),
                              glm::length(glm::vec3(rest[2])));
                const glm::mat3 rotMat(
                    glm::vec3(rest[0]) / std::max(S.x, 1e-8f),
                    glm::vec3(rest[1]) / std::max(S.y, 1e-8f),
                    glm::vec3(rest[2]) / std::max(S.z, 1e-8f));
                R = glm::normalize(glm::quat_cast(rotMat));
            }

            if (!ch.translations.empty())
                T = sampleKeys(ch.translations, time, T, lerpVec);
            if (!ch.rotations.empty())
                R = sampleKeys(ch.rotations, time, R, lerpQuat);
            if (!ch.scales.empty())
                S = sampleKeys(ch.scales, time, S, lerpVec);

            local[ch.jointIndex] = composeTrs(T, R, S);
        }

        std::vector<glm::mat4> global(jointCount, glm::mat4(1.f));
        for (std::uint32_t i = 0; i < jointCount; ++i)
        {
            const auto parent = skeleton.parents[i];
            if (parent >= 0 &&
                static_cast<std::uint32_t>(parent) < jointCount)
                global[i] = global[static_cast<std::uint32_t>(parent)] *
                            local[i];
            else
                global[i] = local[i];
        }

        std::vector<glm::mat4> skin(jointCount, glm::mat4(1.f));
        for (std::uint32_t i = 0; i < jointCount; ++i)
        {
            const glm::mat4 ib =
                i < skeleton.inverseBind.size() ? skeleton.inverseBind[i]
                                                : glm::mat4(1.f);
            skin[i] = global[i] * ib;
        }
        return skin;
    }

} // namespace FREYA_NAMESPACE
