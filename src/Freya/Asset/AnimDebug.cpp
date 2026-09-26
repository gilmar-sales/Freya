#include "Freya/Asset/AnimDebug.hpp"

#include <algorithm>

namespace FREYA_NAMESPACE
{
    SkeletonDebugSnapshot CaptureSkeletonDebug(const Skeleton& skeleton)
    {
        SkeletonDebugSnapshot out;
        const auto            n = skeleton.JointCount();
        out.joints.resize(n);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            out.joints[i].index = i;
            out.joints[i].parent =
                i < skeleton.parents.size() ? skeleton.parents[i] : -1;
            out.joints[i].name =
                i < skeleton.names.size() ? skeleton.names[i] : std::string {};
        }
        return out;
    }

    PoseWorldDebugSnapshot CapturePoseWorldDebug(const Skeleton&  skeleton,
                                                 const LocalPose& local,
                                                 const glm::mat4& modelWorld)
    {
        PoseWorldDebugSnapshot out;
        const auto             globals = LocalToGlobal(skeleton, local);
        const auto n = std::min(skeleton.JointCount(),
                                static_cast<std::uint32_t>(globals.size()));
        out.joints.resize(n);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            out.joints[i].index = i;
            out.joints[i].name =
                i < skeleton.names.size() ? skeleton.names[i] : std::string {};
            out.joints[i].world    = modelWorld * globals[i];
            out.joints[i].position = glm::vec3(out.joints[i].world[3]);
        }
        return out;
    }

    AnimEventRing::AnimEventRing(std::uint32_t capacity) :
        mCap(std::max(1u, capacity))
    {
        mEvents.reserve(mCap);
    }

    void AnimEventRing::Clear()
    {
        mEvents.clear();
        mNext = 0;
        mFull = false;
    }

    void AnimEventRing::Push(FiredAnimationEvent e)
    {
        if (mEvents.size() < mCap)
        {
            mEvents.push_back(std::move(e));
            return;
        }
        mEvents[mNext] = std::move(e);
        mNext          = (mNext + 1u) % mCap;
        mFull          = true;
    }

    void AnimEventRing::PushAll(std::span<const FiredAnimationEvent> events)
    {
        for (const auto& e : events)
            Push(e);
    }

    void AnimEventRing::CopyChronological(
        std::vector<FiredAnimationEvent>& out) const
    {
        out.clear();
        if (mEvents.empty())
            return;
        if (!mFull || mEvents.size() < mCap)
        {
            out = mEvents;
            return;
        }
        out.reserve(mCap);
        for (std::uint32_t i = 0; i < mCap; ++i)
            out.push_back(mEvents[(mNext + i) % mCap]);
    }
} // namespace FREYA_NAMESPACE
