#pragma once

#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <cstdint>
#include <span>
#include <string_view>
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
     * @brief Per-joint overlay weights in [0,1] for layered blending.
     */
    struct BoneMask
    {
        std::vector<float> weights;

        void Resize(std::uint32_t jointCount, float fill = 0.f);
        [[nodiscard]] std::uint32_t Size() const
        {
            return static_cast<std::uint32_t>(weights.size());
        }

        void SetJoint(std::uint32_t joint, float weight);
        /** Set `root` and all descendants to `weight`. */
        void SetSubtree(const Skeleton& skeleton, std::uint32_t root,
                        float weight);

        static BoneMask Filled(std::uint32_t jointCount, float weight);
    };

    /**
     * @brief Root-bone delta between two clip times (for root motion).
     */
    struct RootMotionDelta
    {
        glm::vec3 translation { 0.f };
        glm::quat rotation { 1.f, 0.f, 0.f, 0.f };
    };

    /**
     * @brief Clip sample on a 1D blend axis (e.g. Speed 0/1/2).
     */
    struct Blend1DSample
    {
        float                value         = 0.f;
        const AnimationClip* clip          = nullptr;
        bool                 loop          = true;
        float                playbackSpeed = 1.f;
    };

    /**
     * @brief Adjacent samples + lerp t for a Blend1D parameter.
     *
     * Samples must be sorted ascending by `value`.
     */
    struct Blend1DSpan
    {
        std::uint32_t i0 = 0;
        std::uint32_t i1 = 0;
        float         t  = 0.f; ///< weight toward i1
    };

    /** First joint whose name contains `needle`, or -1. */
    [[nodiscard]] std::int32_t FindJointIndex(const Skeleton&  skeleton,
                                              std::string_view needle);

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

    /**
     * @brief Layer `overlay` onto `base` using per-joint mask * layerWeight.
     *
     * `out[j] = lerp(base[j], overlay[j], mask[j] * layerWeight)`.
     */
    LocalPose BlendMasked(const LocalPose& base, const LocalPose& overlay,
                          const BoneMask& mask, float layerWeight = 1.f);

    /**
     * @brief Additive layer: apply (additive − reference) onto `base`.
     *
     * Rotation: `out = slerp(I, add * inverse(ref), w) * base`.
     * Translation/scale: `base + (add − ref) * w`.
     * Optional mask scales per-joint weight (same idea as BlendMasked).
     */
    LocalPose BlendAdditive(const LocalPose& base, const LocalPose& additive,
                            const LocalPose& reference, float weight);

    LocalPose BlendAdditive(const LocalPose& base, const LocalPose& additive,
                            const LocalPose& reference, const BoneMask& mask,
                            float layerWeight = 1.f);

    /**
     * @brief Resolve which two Blend1D samples to lerp for `param`.
     *
     * `values` must be sorted ascending. Empty → {0,0,0}.
     */
    Blend1DSpan ResolveBlend1D(std::span<const float> values, float param);

    /**
     * @brief Sample and blend clips along a 1D parameter (Idle↔Walk↔Run).
     *
     * `times` is per-sample playback time. Resized to samples.size() if empty.
     */
    LocalPose EvaluateBlend1D(const Skeleton&                skeleton,
                              std::span<const Blend1DSample> samples,
                              std::span<float> times, float param);

    /** Advance each sample's time by dt * playbackSpeed (with loop wrap). */
    void AdvanceBlend1DTimes(std::span<const Blend1DSample> samples,
                             std::span<float> times, float dt);

    /**
     * @brief Advance a shared normalized phase for Blend1D locomotion sync.
     *
     * Uses the active span at `param`: phase rate is the lerp of the two
     * samples' (playbackSpeed / duration). Keeps Walk/Run feet aligned while
     * blending. Returns the wrapped phase in [0,1).
     */
    [[nodiscard]] float AdvanceBlend1DPhase(
        std::span<const Blend1DSample> samples, float param, float phase,
        float dt);

    /** Write times[i] = phase * duration[i] for every sample with a clip. */
    void WriteBlend1DTimesFromPhase(std::span<const Blend1DSample> samples,
                                    std::span<float> times, float phase);

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
