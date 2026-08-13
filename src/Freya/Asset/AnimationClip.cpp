#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/Pose.hpp"

#include <algorithm>
#include <cmath>

namespace FREYA_NAMESPACE
{
    namespace
    {
        float wrapTime(float t, const float duration)
        {
            if (duration <= 0.f)
                return std::max(0.f, t);
            t = std::fmod(t, duration);
            if (t < 0.f)
                t += duration;
            return t;
        }

        void collectRange(const AnimationClip& clip, const float fromExclusive,
                          const float                  toInclusive,
                          std::vector<AnimationEvent>& out)
        {
            for (const auto& e : clip.events)
            {
                if (e.timeSec > fromExclusive && e.timeSec <= toInclusive)
                    out.push_back(e);
            }
        }
    } // namespace

    void CollectClipEvents(const AnimationClip& clip, float tPrev, float tCurr,
                           const bool loop, std::vector<AnimationEvent>& out)
    {
        if (clip.events.empty())
            return;

        if (!loop || clip.duration <= 0.f)
        {
            if (tCurr < tPrev)
                std::swap(tPrev, tCurr);
            collectRange(clip, tPrev, tCurr, out);
            return;
        }

        tPrev = wrapTime(tPrev, clip.duration);
        tCurr = wrapTime(tCurr, clip.duration);

        if (tCurr >= tPrev)
            collectRange(clip, tPrev, tCurr, out);
        else
        {
            // Wrapped: (tPrev, duration] U [0, tCurr] — 0 needs inclusive for
            // events at exactly 0 after wrap; use (-eps, tCurr].
            collectRange(clip, tPrev, clip.duration, out);
            collectRange(clip, -1e-5f, tCurr, out);
        }
    }

    void CollectFiredClipEvents(const AnimationClip& clip, const float tPrev,
                                const float tCurr, const bool loop,
                                std::vector<FiredAnimationEvent>& out)
    {
        std::vector<AnimationEvent> raw;
        CollectClipEvents(clip, tPrev, tCurr, loop, raw);
        out.reserve(out.size() + raw.size());
        for (auto& e : raw)
        {
            out.push_back(FiredAnimationEvent {
                .name    = std::move(e.name),
                .timeSec = e.timeSec,
                .clip    = &clip,
            });
        }
    }

    void EnsureDefaultFootstepEvents(AnimationClip& clip)
    {
        if (!clip.events.empty() || clip.duration <= 0.f)
            return;
        clip.events.push_back(
            AnimationEvent { "Footstep.L", clip.duration * 0.15f });
        clip.events.push_back(
            AnimationEvent { "Footstep.R", clip.duration * 0.65f });
    }

    std::vector<glm::mat4> EvaluateSkeletonPose(
        const Skeleton& skeleton, const AnimationClip& clip, float timeSec)
    {
        return PoseToSkinMatrices(skeleton,
                                  SampleClip(skeleton, clip, timeSec, true));
    }

} // namespace FREYA_NAMESPACE
