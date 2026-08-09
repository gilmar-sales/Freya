#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FREYA_NAMESPACE
{
    struct AnimationVecKey
    {
        float     time = 0.f;
        glm::vec3 value { 0.f };
    };

    struct AnimationQuatKey
    {
        float      time = 0.f;
        glm::quat  value { 1.f, 0.f, 0.f, 0.f };
    };

    /**
     * @brief Per-joint TRS track (times in seconds).
     */
    struct AnimationChannel
    {
        std::uint32_t                 jointIndex = 0;
        std::vector<AnimationVecKey>  translations;
        std::vector<AnimationQuatKey> rotations;
        std::vector<AnimationVecKey>  scales;
    };

    /**
     * @brief Skeletal animation clip sampled on the CPU.
     */
    struct AnimationClip
    {
        std::string                   name;
        float                         duration = 0.f; ///< seconds
        float                         ticksPerSecond = 25.f;
        std::vector<AnimationChannel> channels;
    };

    struct Skeleton;

    /**
     * @brief Evaluate clip at `timeSec` (looping) into skin matrices
     *        (global * inverseBind) sized to skeleton.JointCount().
     */
    std::vector<glm::mat4> EvaluateSkeletonPose(const Skeleton&      skeleton,
                                                const AnimationClip& clip,
                                                float                timeSec);

} // namespace FREYA_NAMESPACE
