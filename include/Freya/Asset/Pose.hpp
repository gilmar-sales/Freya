#pragma once

#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Per-joint local TRS (animation / blend space).
     */
    struct JointTRS
    {
        glm::vec3 translation { 0.f };
        glm::quat rotation { 1.f, 0.f, 0.f, 0.f };
        glm::vec3 scale { 1.f };

        [[nodiscard]] glm::mat4 ToMatrix() const;
    };

    /**
     * @brief Local-space pose (one JointTRS per skeleton joint).
     */
    struct LocalPose
    {
        std::vector<JointTRS> joints;

        void                        Resize(std::uint32_t jointCount);
        [[nodiscard]] std::uint32_t Size() const
        {
            return static_cast<std::uint32_t>(joints.size());
        }
    };

    /**
     * @brief Root-bone delta between two clip times (for root motion).
     */
    struct RootMotionDelta
    {
        glm::vec3 translation { 0.f };
        glm::quat rotation { 1.f, 0.f, 0.f, 0.f };
    };

    /** Rest pose from skeleton.restLocal (decomposed). */
    LocalPose RestLocalPose(const Skeleton& skeleton);

    /**
     * @brief Sample clip into local TRS (looping when loop=true).
     *
     * Missing tracks keep the rest-pose component.
     */
    LocalPose SampleClip(const Skeleton& skeleton, const AnimationClip& clip,
                         float timeSec, bool loop = true);

    /** Nlerp / slerp blend of two local poses (t in [0,1]). */
    LocalPose BlendLocalPoses(const LocalPose& a, const LocalPose& b, float t);

    /** Local → global joint matrices. */
    std::vector<glm::mat4> LocalToGlobal(const Skeleton&  skeleton,
                                         const LocalPose& local);

    /** Local → GPU skin matrices (global * inverseBind). */
    std::vector<glm::mat4> PoseToSkinMatrices(const Skeleton&  skeleton,
                                              const LocalPose& local);

    /**
     * @brief Delta of the first root joint (parent < 0) between t0 and t1.
     *
     * Handles looping when `loop` and duration > 0 (unwraps wrap-around).
     */
    RootMotionDelta ExtractRootDelta(const Skeleton&      skeleton,
                                     const AnimationClip& clip, float t0,
                                     float t1, bool loop = true);

} // namespace FREYA_NAMESPACE
