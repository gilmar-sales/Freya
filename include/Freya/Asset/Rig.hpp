#pragma once

#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Two-bone IK chain (e.g. hip–knee–foot / shoulder–elbow–hand).
     */
    struct TwoBoneChain
    {
        std::uint32_t root = 0; ///< upper bone
        std::uint32_t mid  = 0; ///< mid bone
        std::uint32_t tip  = 0; ///< end effector
    };

    /**
     * @brief Rotate `joint` (in local pose) so it looks toward a world target.
     *
     * Uses the joint's current global forward (`localForward` in joint space,
     * typically +Z for glTF). Clamps yaw/pitch relative to the animated pose.
     * `weight` in [0,1] blends the correction. Pass `maxYawRad` < 0 to
     * skip swing clamp (raw aim).
     *
     * @return false if indices / pose are invalid.
     */
    bool ApplyLookAt(const Skeleton& skeleton, LocalPose& local,
                     const glm::mat4& modelWorld, std::uint32_t joint,
                     const glm::vec3& targetWorld, float weight = 1.f,
                     float maxYawRad = 1.2f, float maxPitchRad = 0.8f,
                     glm::vec3 localForward = glm::vec3(0.f, 0.f, 1.f));

    /**
     * @brief Analytical two-bone IK toward a world-space tip target.
     *
     * `poleWorld` bends the mid joint (knee/elbow). `weight` blends vs
     * animated.
     *
     * @return false if unreachable setup / invalid indices.
     */
    bool SolveTwoBoneIK(const Skeleton& skeleton, LocalPose& local,
                        const glm::mat4& modelWorld, const TwoBoneChain& chain,
                        const glm::vec3& targetWorld,
                        const glm::vec3& poleWorld, float weight = 1.f);

    /**
     * @brief Apply root motion delta to an instance model matrix.
     *
     * When `planar` is true, only XZ translation is applied (Y ignored) and
     * yaw from `delta.rotation` is applied around world up.
     */
    [[nodiscard]] glm::mat4 IntegrateRootMotion(const glm::mat4&       model,
                                                const RootMotionDelta& delta,
                                                bool planar = true);

    /**
     * @brief Zero XZ translation on the skeleton root joint (after consuming
     *        it into the instance transform).
     */
    void CancelRootTranslationXZ(const Skeleton& skeleton, LocalPose& local);

    /**
     * @brief First parentless joint index, or -1.
     */
    [[nodiscard]] std::int32_t FindRootJoint(const Skeleton& skeleton);

    /**
     * @brief Procedural planar locomotion (for in-place clips).
     *
     * Moves `model` along its local forward (−Z by default) by
     * `metersPerSec * dt`. Useful when authored clips have no root curves.
     */
    [[nodiscard]] glm::mat4 DrivePlanarLocomotion(
        const glm::mat4& model, float metersPerSec, float dt,
        glm::vec3 localForward = glm::vec3(0.f, 0.f, -1.f));

} // namespace FREYA_NAMESPACE
