#pragma once

#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Skeleton.hpp"

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

    [[nodiscard]] SkeletonDebugSnapshot CaptureSkeletonDebug(
        const Skeleton& skeleton);

    [[nodiscard]] PoseWorldDebugSnapshot CapturePoseWorldDebug(
        const Skeleton& skeleton, const LocalPose& local,
        const glm::mat4& modelWorld);

    /**
     * @brief Fixed-capacity ring of recent #FiredAnimationEvent for UI logs.
     *
     * Agnostic — push after Advance/Evaluate; UI reads oldest→newest or
     * reverse.
     */
    class AnimEventRing
    {
      public:
        explicit AnimEventRing(std::uint32_t capacity = 64);

        void Clear();

        void Push(FiredAnimationEvent e);

        void PushAll(std::span<const FiredAnimationEvent> events);

        [[nodiscard]] std::uint32_t Capacity() const { return mCap; }
        [[nodiscard]] std::uint32_t Size() const
        {
            return static_cast<std::uint32_t>(mEvents.size());
        }
        [[nodiscard]] bool Full() const { return mFull; }

        /**
         * @brief Copy chronologically (oldest first) into `out`.
         */
        void CopyChronological(std::vector<FiredAnimationEvent>& out) const;

      private:
        std::uint32_t                    mCap  = 64;
        std::uint32_t                    mNext = 0;
        bool                             mFull = false;
        std::vector<FiredAnimationEvent> mEvents;
    };

} // namespace FREYA_NAMESPACE
