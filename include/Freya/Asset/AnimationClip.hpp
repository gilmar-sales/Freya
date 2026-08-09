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
        float     time = 0.f;
        glm::quat value { 1.f, 0.f, 0.f, 0.f };
    };

    /**
     * @brief Named marker on a clip timeline (seconds).
     */
    struct AnimationEvent
    {
        std::string name;
        float       timeSec = 0.f;
    };

    struct AnimationClip;

    /**
     * @brief Event that fired while advancing time over a frame.
     */
    struct FiredAnimationEvent
    {
        std::string          name;
        float                timeSec = 0.f;
        const AnimationClip* clip    = nullptr;
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
        float                         duration       = 0.f; ///< seconds
        float                         ticksPerSecond = 25.f;
        std::vector<AnimationChannel> channels;
        std::vector<AnimationEvent>   events;
    };

    struct Skeleton;

    /**
     * @brief Append clip events with time in (tPrev, tCurr] (seconds).
     *
     * Handles looping wrap when `loop` and duration > 0.
     */
    void CollectClipEvents(const AnimationClip& clip, float tPrev, float tCurr,
                           bool loop, std::vector<AnimationEvent>& out);

    /**
     * @brief Same as CollectClipEvents, tagging `clip` on each fired event.
     */
    void CollectFiredClipEvents(const AnimationClip& clip, float tPrev,
                                float tCurr, bool loop,
                                std::vector<FiredAnimationEvent>& out);

    /**
     * @brief If `events` is empty, insert L/R footsteps at ~15% / 65% duration.
     *
     * Useful for Walk/Run clips that have no authored markers (e.g. Fox.glb).
     */
    void EnsureDefaultFootstepEvents(AnimationClip& clip);

    /**
     * @brief Evaluate clip at `timeSec` (looping) into skin matrices
     *        (global * inverseBind) sized to skeleton.JointCount().
     */
    std::vector<glm::mat4> EvaluateSkeletonPose(
        const Skeleton& skeleton, const AnimationClip& clip, float timeSec);

} // namespace FREYA_NAMESPACE
