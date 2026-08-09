#pragma once

#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Toolkit-agnostic skeleton table for UI joint pickers.
     */
    struct SkeletonDebugSnapshot
    {
        struct Joint
        {
            std::uint32_t index  = 0;
            std::int32_t  parent = -1;
            std::string   name;
        };

        std::vector<Joint> joints;

        [[nodiscard]] std::uint32_t JointCount() const
        {
            return static_cast<std::uint32_t>(joints.size());
        }
    };

    /**
     * @brief World-space joint debug (from local pose + model).
     *
     * Positions are translation of `modelWorld * global[i]`.
     */
    struct PoseWorldDebugSnapshot
    {
        struct Joint
        {
            std::uint32_t index = 0;
            std::string   name;
            glm::vec3     position { 0.f };
            glm::mat4     world { 1.f }; ///< modelWorld * global
        };

        std::vector<Joint> joints;
    };

    [[nodiscard]] inline SkeletonDebugSnapshot CaptureSkeletonDebug(
        const Skeleton& skeleton)
    {
        SkeletonDebugSnapshot out;
        const auto            n = skeleton.JointCount();
        out.joints.resize(n);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            out.joints[i].index  = i;
            out.joints[i].parent =
                i < skeleton.parents.size() ? skeleton.parents[i] : -1;
            out.joints[i].name =
                i < skeleton.names.size() ? skeleton.names[i] : std::string {};
        }
        return out;
    }

    [[nodiscard]] inline PoseWorldDebugSnapshot CapturePoseWorldDebug(
        const Skeleton& skeleton, const LocalPose& local,
        const glm::mat4& modelWorld)
    {
        PoseWorldDebugSnapshot out;
        const auto             globals = LocalToGlobal(skeleton, local);
        const auto             n =
            std::min(skeleton.JointCount(),
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

    /**
     * @brief Fixed-capacity ring of recent #FiredAnimationEvent for UI logs.
     *
     * Agnostic — push after Advance/Evaluate; UI reads oldest→newest or
     * reverse.
     */
    class AnimEventRing
    {
      public:
        explicit AnimEventRing(const std::uint32_t capacity = 64) :
            mCap(std::max(1u, capacity))
        {
            mEvents.reserve(mCap);
        }

        void Clear()
        {
            mEvents.clear();
            mNext = 0;
            mFull = false;
        }

        void Push(FiredAnimationEvent e)
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

        void PushAll(std::span<const FiredAnimationEvent> events)
        {
            for (const auto& e : events)
                Push(e);
        }

        [[nodiscard]] std::uint32_t Capacity() const { return mCap; }
        [[nodiscard]] std::uint32_t Size() const
        {
            return static_cast<std::uint32_t>(mEvents.size());
        }
        [[nodiscard]] bool Full() const { return mFull; }

        /**
         * @brief Copy chronologically (oldest first) into `out`.
         */
        void CopyChronological(std::vector<FiredAnimationEvent>& out) const
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

      private:
        std::uint32_t                    mCap  = 64;
        std::uint32_t                    mNext = 0;
        bool                             mFull = false;
        std::vector<FiredAnimationEvent> mEvents;
    };

} // namespace FREYA_NAMESPACE
