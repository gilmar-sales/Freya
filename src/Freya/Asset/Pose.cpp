#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/BakedAnimation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

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

    void BoneMask::Resize(const std::uint32_t jointCount, const float fill)
    {
        weights.assign(jointCount, fill);
    }

    void BoneMask::SetJoint(const std::uint32_t joint, const float weight)
    {
        if (joint < weights.size())
            weights[joint] = std::clamp(weight, 0.f, 1.f);
    }

    void BoneMask::SetSubtree(const Skeleton&     skeleton,
                              const std::uint32_t root, const float weight)
    {
        const auto n = skeleton.JointCount();
        if (weights.size() < n)
            Resize(n, 0.f);
        if (root >= n)
            return;

        const float w = std::clamp(weight, 0.f, 1.f);
        weights[root] = w;
        for (std::uint32_t i = 0; i < n; ++i)
        {
            auto p = skeleton.parents[i];
            while (p >= 0)
            {
                if (static_cast<std::uint32_t>(p) == root)
                {
                    weights[i] = w;
                    break;
                }
                p = skeleton.parents[static_cast<std::uint32_t>(p)];
            }
        }
    }

    BoneMask BoneMask::Filled(const std::uint32_t jointCount,
                              const float         weight)
    {
        BoneMask m;
        m.Resize(jointCount, std::clamp(weight, 0.f, 1.f));
        return m;
    }

    std::int32_t FindJointIndex(const Skeleton&        skeleton,
                                const std::string_view needle)
    {
        if (needle.empty())
            return -1;
        for (std::uint32_t i = 0; i < skeleton.names.size(); ++i)
        {
            if (skeleton.names[i].find(needle) != std::string::npos)
                return static_cast<std::int32_t>(i);
        }
        return -1;
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

    LocalPose BlendMasked(const LocalPose& base, const LocalPose& overlay,
                          const BoneMask& mask, const float layerWeight)
    {
        const float layer = std::clamp(layerWeight, 0.f, 1.f);
        if (layer <= 0.f)
            return base;

        LocalPose  out;
        const auto n = std::min({ base.Size(), overlay.Size(), mask.Size() });
        out.Resize(n);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            const float u = std::clamp(mask.weights[i], 0.f, 1.f) * layer;
            if (u <= 0.f)
            {
                out.joints[i] = base.joints[i];
                continue;
            }
            if (u >= 1.f)
            {
                out.joints[i] = overlay.joints[i];
                continue;
            }
            out.joints[i].translation = glm::mix(
                base.joints[i].translation, overlay.joints[i].translation, u);
            out.joints[i].rotation = glm::normalize(glm::slerp(
                base.joints[i].rotation, overlay.joints[i].rotation, u));
            out.joints[i].scale =
                glm::mix(base.joints[i].scale, overlay.joints[i].scale, u);
        }
        return out;
    }

    namespace
    {
        JointTRS applyAdditiveJoint(const JointTRS& base, const JointTRS& add,
                                    const JointTRS& ref, const float w)
        {
            JointTRS out;
            out.translation =
                base.translation + (add.translation - ref.translation) * w;
            out.scale = base.scale + (add.scale - ref.scale) * w;
            const glm::quat delta =
                glm::normalize(add.rotation * glm::inverse(ref.rotation));
            const glm::quat id(1.f, 0.f, 0.f, 0.f);
            out.rotation =
                glm::normalize(glm::slerp(id, delta, w) * base.rotation);
            return out;
        }
    } // namespace

    LocalPose BlendAdditive(const LocalPose& base, const LocalPose& additive,
                            const LocalPose& reference, const float weight)
    {
        const float w = std::clamp(weight, 0.f, 1.f);
        if (w <= 0.f)
            return base;

        LocalPose  out;
        const auto n =
            std::min({ base.Size(), additive.Size(), reference.Size() });
        out.Resize(n);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            if (w >= 1.f)
                out.joints[i] =
                    applyAdditiveJoint(base.joints[i], additive.joints[i],
                                       reference.joints[i], 1.f);
            else
                out.joints[i] = applyAdditiveJoint(
                    base.joints[i], additive.joints[i], reference.joints[i], w);
        }
        return out;
    }

    LocalPose BlendAdditive(const LocalPose& base, const LocalPose& additive,
                            const LocalPose& reference, const BoneMask& mask,
                            const float layerWeight)
    {
        const float layer = std::clamp(layerWeight, 0.f, 1.f);
        if (layer <= 0.f)
            return base;

        LocalPose  out;
        const auto n = std::min(
            { base.Size(), additive.Size(), reference.Size(), mask.Size() });
        out.Resize(n);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            const float w = std::clamp(mask.weights[i], 0.f, 1.f) * layer;
            if (w <= 0.f)
            {
                out.joints[i] = base.joints[i];
                continue;
            }
            out.joints[i] = applyAdditiveJoint(
                base.joints[i], additive.joints[i], reference.joints[i], w);
        }
        return out;
    }

    Blend1DSpan ResolveBlend1D(const std::span<const float> values,
                               const float                  param)
    {
        Blend1DSpan span {};
        if (values.empty())
            return span;
        if (values.size() == 1 || param <= values.front())
            return span;
        if (param >= values.back())
        {
            span.i0 = span.i1 = static_cast<std::uint32_t>(values.size() - 1);
            span.t            = 0.f;
            return span;
        }

        std::uint32_t next = 1;
        while (next < values.size() && param > values[next])
            ++next;
        span.i0       = next - 1;
        span.i1       = next;
        const float a = values[span.i0];
        const float b = values[span.i1];
        span.t = (b > a) ? std::clamp((param - a) / (b - a), 0.f, 1.f) : 0.f;
        return span;
    }

    void AdvanceBlend1DTimes(const std::span<const Blend1DSample> samples,
                             const std::span<float> times, const float dt)
    {
        if (times.size() < samples.size())
            return;
        for (std::uint32_t i = 0; i < samples.size(); ++i)
        {
            const auto& s = samples[i];
            if (!s.clip)
                continue;
            times[i] += dt * std::max(s.playbackSpeed, 0.f);
            if (s.loop && s.clip->duration > 0.f)
            {
                times[i] = std::fmod(times[i], s.clip->duration);
                if (times[i] < 0.f)
                    times[i] += s.clip->duration;
            }
        }
    }

    float AdvanceBlend1DPhase(const std::span<const Blend1DSample> samples,
                              const float param, float phase, const float dt)
    {
        if (samples.empty() || dt <= 0.f)
            return phase;

        std::vector<float> values(samples.size());
        for (std::uint32_t i = 0; i < samples.size(); ++i)
            values[i] = samples[i].value;

        const auto  span = ResolveBlend1D(values, param);
        const auto& s0   = samples[span.i0];
        const auto& s1   = samples[span.i1];

        const float d0 =
            (s0.clip && s0.clip->duration > 0.f) ? s0.clip->duration : 0.f;
        const float d1 =
            (s1.clip && s1.clip->duration > 0.f) ? s1.clip->duration : 0.f;
        const float r0 = std::max(s0.playbackSpeed, 0.f);
        const float r1 = std::max(s1.playbackSpeed, 0.f);

        float duration = glm::mix(d0, d1, span.t);
        float rate     = glm::mix(r0, r1, span.t);
        if (duration <= 1e-5f)
        {
            // Fall back to whichever sample has a usable length.
            duration = std::max(d0, d1);
            rate     = (d0 >= d1) ? r0 : r1;
        }
        if (duration <= 1e-5f)
            return phase;

        phase += dt * rate / duration;
        phase = std::fmod(phase, 1.f);
        if (phase < 0.f)
            phase += 1.f;
        return phase;
    }

    void WriteBlend1DTimesFromPhase(
        const std::span<const Blend1DSample> samples,
        const std::span<float> times, const float phase)
    {
        if (times.size() < samples.size())
            return;
        const float p = std::clamp(phase, 0.f, 1.f);
        for (std::uint32_t i = 0; i < samples.size(); ++i)
        {
            const auto& s = samples[i];
            if (!s.clip || s.clip->duration <= 0.f)
            {
                times[i] = 0.f;
                continue;
            }
            times[i] = p * s.clip->duration;
        }
    }

    LocalPose EvaluateBlend1D(const Skeleton&                      skeleton,
                              const std::span<const Blend1DSample> samples,
                              const std::span<float> times, const float param)
    {
        if (samples.empty())
            return RestLocalPose(skeleton);

        std::vector<float> values(samples.size());
        for (std::uint32_t i = 0; i < samples.size(); ++i)
            values[i] = samples[i].value;

        const auto  span = ResolveBlend1D(values, param);
        const auto& s0   = samples[span.i0];
        const auto& s1   = samples[span.i1];
        const float t0   = span.i0 < times.size() ? times[span.i0] : 0.f;
        const float t1   = span.i1 < times.size() ? times[span.i1] : 0.f;

        LocalPose a;
        if (s0.baked && !s0.baked->Empty())
            a = SampleBaked(skeleton, *s0.baked, t0, s0.loop);
        else if (s0.clip)
            a = SampleClip(skeleton, *s0.clip, t0, s0.loop);
        else
            a = RestLocalPose(skeleton);
        if (span.i0 == span.i1 || span.t <= 0.f)
            return a;
        LocalPose b;
        if (s1.baked && !s1.baked->Empty())
            b = SampleBaked(skeleton, *s1.baked, t1, s1.loop);
        else if (s1.clip)
            b = SampleClip(skeleton, *s1.clip, t1, s1.loop);
        else
            b = RestLocalPose(skeleton);
        return BlendLocalPoses(a, b, span.t);
    }

    namespace
    {
        bool pointInTriangle(const glm::vec2& p, const glm::vec2& a,
                             const glm::vec2& b, const glm::vec2& c, float& w0,
                             float& w1, float& w2)
        {
            const glm::vec2 v0  = b - a;
            const glm::vec2 v1  = c - a;
            const glm::vec2 v2  = p - a;
            const float     d00 = glm::dot(v0, v0);
            const float     d01 = glm::dot(v0, v1);
            const float     d11 = glm::dot(v1, v1);
            const float     d20 = glm::dot(v2, v0);
            const float     d21 = glm::dot(v2, v1);
            const float     den = d00 * d11 - d01 * d01;
            if (std::abs(den) < 1e-12f)
                return false;
            const float inv     = 1.f / den;
            w1                  = (d11 * d20 - d01 * d21) * inv;
            w2                  = (d00 * d21 - d01 * d20) * inv;
            w0                  = 1.f - w1 - w2;
            constexpr float eps = -1e-4f;
            return w0 >= eps && w1 >= eps && w2 >= eps;
        }

        LocalPose sampleBlend2DSample(const Skeleton&      skeleton,
                                      const Blend2DSample& s, const float time)
        {
            if (s.baked && !s.baked->Empty())
                return SampleBaked(skeleton, *s.baked, time, s.loop);
            if (s.clip)
                return SampleClip(skeleton, *s.clip, time, s.loop);
            return RestLocalPose(skeleton);
        }
    } // namespace

    Blend2DTriangle ResolveBlend2D(const std::span<const glm::vec2> positions,
                                   const glm::vec2                  param)
    {
        Blend2DTriangle out {};
        if (positions.empty())
            return out;
        if (positions.size() == 1)
            return out;

        if (positions.size() == 2)
        {
            const glm::vec2 a    = positions[0];
            const glm::vec2 b    = positions[1];
            const glm::vec2 ab   = b - a;
            const float     len2 = glm::dot(ab, ab);
            float           t    = 0.f;
            if (len2 > 1e-12f)
                t = std::clamp(glm::dot(param - a, ab) / len2, 0.f, 1.f);
            out.i0 = 0;
            out.i1 = 1;
            out.i2 = 0;
            out.w0 = 1.f - t;
            out.w1 = t;
            out.w2 = 0.f;
            return out;
        }

        float bestArea = std::numeric_limits<float>::max();
        bool  found    = false;
        for (std::uint32_t i = 0; i < positions.size(); ++i)
        {
            for (std::uint32_t j = i + 1; j < positions.size(); ++j)
            {
                for (std::uint32_t k = j + 1; k < positions.size(); ++k)
                {
                    float w0 = 0.f, w1 = 0.f, w2 = 0.f;
                    if (!pointInTriangle(param, positions[i], positions[j],
                                         positions[k], w0, w1, w2))
                        continue;
                    const glm::vec2 e1 = positions[j] - positions[i];
                    const glm::vec2 e2 = positions[k] - positions[i];
                    const float     area =
                        std::abs(e1.x * e2.y - e1.y * e2.x) * 0.5f;
                    if (area < 1e-10f || area >= bestArea)
                        continue;
                    bestArea        = area;
                    found           = true;
                    out.i0          = i;
                    out.i1          = j;
                    out.i2          = k;
                    out.w0          = std::max(0.f, w0);
                    out.w1          = std::max(0.f, w1);
                    out.w2          = std::max(0.f, w2);
                    const float sum = out.w0 + out.w1 + out.w2;
                    if (sum > 1e-6f)
                    {
                        out.w0 /= sum;
                        out.w1 /= sum;
                        out.w2 /= sum;
                    }
                }
            }
        }
        if (found)
            return out;

        // Outside hull: snap to nearest sample.
        float         bestD = std::numeric_limits<float>::max();
        std::uint32_t bestI = 0;
        for (std::uint32_t i = 0; i < positions.size(); ++i)
        {
            const float d =
                glm::dot(param - positions[i], param - positions[i]);
            if (d < bestD)
            {
                bestD = d;
                bestI = i;
            }
        }
        out.i0 = out.i1 = out.i2 = bestI;
        out.w0                   = 1.f;
        out.w1 = out.w2 = 0.f;
        return out;
    }

    void AdvanceBlend2DTimes(const std::span<const Blend2DSample> samples,
                             const std::span<float> times, const float dt)
    {
        if (times.size() < samples.size())
            return;
        for (std::uint32_t i = 0; i < samples.size(); ++i)
        {
            const auto& s = samples[i];
            if (!s.clip)
                continue;
            times[i] += dt * std::max(s.playbackSpeed, 0.f);
            if (s.loop && s.clip->duration > 0.f)
            {
                times[i] = std::fmod(times[i], s.clip->duration);
                if (times[i] < 0.f)
                    times[i] += s.clip->duration;
            }
        }
    }

    float AdvanceBlend2DPhase(const std::span<const Blend2DSample> samples,
                              const glm::vec2 param, float phase,
                              const float dt)
    {
        if (samples.empty() || dt <= 0.f)
            return phase;

        std::vector<glm::vec2> positions(samples.size());
        for (std::uint32_t i = 0; i < samples.size(); ++i)
            positions[i] = samples[i].pos;
        const auto tri = ResolveBlend2D(positions, param);

        const auto durationOf = [&](const std::uint32_t idx) {
            const auto& s = samples[idx];
            return (s.clip && s.clip->duration > 0.f) ? s.clip->duration : 0.f;
        };
        const auto rateOf = [&](const std::uint32_t idx) {
            return std::max(samples[idx].playbackSpeed, 0.f);
        };

        float duration =
            durationOf(tri.i0) * tri.w0 + durationOf(tri.i1) * tri.w1 +
            durationOf(tri.i2) * tri.w2;
        float rate = rateOf(tri.i0) * tri.w0 + rateOf(tri.i1) * tri.w1 +
                     rateOf(tri.i2) * tri.w2;
        if (duration <= 1e-5f)
            return phase;

        phase += dt * rate / duration;
        phase = std::fmod(phase, 1.f);
        if (phase < 0.f)
            phase += 1.f;
        return phase;
    }

    void WriteBlend2DTimesFromPhase(
        const std::span<const Blend2DSample> samples,
        const std::span<float> times, const float phase)
    {
        if (times.size() < samples.size())
            return;
        const float p = std::clamp(phase, 0.f, 1.f);
        for (std::uint32_t i = 0; i < samples.size(); ++i)
        {
            const auto& s = samples[i];
            if (!s.clip || s.clip->duration <= 0.f)
            {
                times[i] = 0.f;
                continue;
            }
            times[i] = p * s.clip->duration;
        }
    }

    LocalPose EvaluateBlend2D(const Skeleton& skeleton,
                              const std::span<const Blend2DSample>
                                  samples,
                              const std::span<float>
                                              times,
                              const glm::vec2 param)
    {
        if (samples.empty())
            return RestLocalPose(skeleton);

        std::vector<glm::vec2> positions(samples.size());
        for (std::uint32_t i = 0; i < samples.size(); ++i)
            positions[i] = samples[i].pos;
        const auto tri = ResolveBlend2D(positions, param);

        const float t0 = tri.i0 < times.size() ? times[tri.i0] : 0.f;
        const float t1 = tri.i1 < times.size() ? times[tri.i1] : 0.f;
        const float t2 = tri.i2 < times.size() ? times[tri.i2] : 0.f;

        LocalPose a = sampleBlend2DSample(skeleton, samples[tri.i0], t0);
        if (tri.w1 <= 1e-6f && tri.w2 <= 1e-6f)
            return a;
        LocalPose b = sampleBlend2DSample(skeleton, samples[tri.i1], t1);
        if (tri.w2 <= 1e-6f)
        {
            const float sum = tri.w0 + tri.w1;
            const float t =
                sum > 1e-6f ? std::clamp(tri.w1 / sum, 0.f, 1.f) : 0.f;
            return BlendLocalPoses(a, b, t);
        }
        LocalPose c = sampleBlend2DSample(skeleton, samples[tri.i2], t2);
        // Nested lerp preserving barycentric weights.
        const float w01 = tri.w0 + tri.w1;
        LocalPose   ab =
            w01 > 1e-6f
                ? BlendLocalPoses(a, b, std::clamp(tri.w1 / w01, 0.f, 1.f))
                : a;
        return BlendLocalPoses(ab, c, std::clamp(tri.w2, 0.f, 1.f));
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
