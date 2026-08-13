#pragma once

#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <cstdint>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Uniform-rate local-pose bake of an AnimationClip.
     *
     * Layout is frame-major: joint at (frame, j) is
     * `joints[frame * jointCount + j]`. Looping clips store `frameCount`
     * samples over [0, duration) so neighbors wrap with `% frameCount`.
     */
    struct BakedClip
    {
        float                 duration   = 0.f;
        float                 frameDt    = 0.f; ///< 1 / bakeHz (metadata)
        std::uint32_t         jointCount = 0;
        std::uint32_t         frameCount = 0;
        std::vector<JointTRS> joints;

        [[nodiscard]] bool Empty() const
        {
            return frameCount == 0 || jointCount == 0 || joints.empty();
        }
    };

    /**
     * @brief Sample `clip` at a fixed rate into a dense local-pose table.
     *
     * `bakeHz` is clamped to at least 1. Zero-duration / empty skeletons
     * produce a single rest-pose frame.
     */
    [[nodiscard]] BakedClip BakeClip(const Skeleton&      skeleton,
                                     const AnimationClip& clip,
                                     float                bakeHz = 30.f);

    /**
     * @brief Reconstruct a local pose from a bake (lerp + slerp).
     *
     * When `loop` and duration > 0, time wraps and frame indices wrap.
     * Missing / empty bakes fall back to `RestLocalPose`.
     */
    [[nodiscard]] LocalPose SampleBaked(const Skeleton&  skeleton,
                                        const BakedClip& baked, float timeSec,
                                        bool loop = true);

} // namespace FREYA_NAMESPACE
