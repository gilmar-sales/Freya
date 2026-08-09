#include "Freya/Asset/Rig.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FREYA_NAMESPACE
{
    namespace
    {
        glm::quat quatFromTo(glm::vec3 from, glm::vec3 to)
        {
            from          = glm::normalize(from);
            to            = glm::normalize(to);
            const float d = glm::dot(from, to);
            if (d > 0.9999f)
                return glm::quat(1.f, 0.f, 0.f, 0.f);
            if (d < -0.9999f)
            {
                glm::vec3 axis = glm::cross(glm::vec3(1.f, 0.f, 0.f), from);
                if (glm::dot(axis, axis) < 1e-8f)
                    axis = glm::cross(glm::vec3(0.f, 1.f, 0.f), from);
                return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
            }
            const glm::vec3 axis = glm::cross(from, to);
            glm::quat       q(d + 1.f, axis.x, axis.y, axis.z);
            return glm::normalize(q);
        }

        glm::mat4 parentWorld(const Skeleton&               skeleton,
                              const std::vector<glm::mat4>& global,
                              const std::uint32_t           joint,
                              const glm::mat4&              modelWorld)
        {
            if (joint >= skeleton.parents.size())
                return modelWorld;
            const auto p = skeleton.parents[joint];
            if (p < 0)
                return modelWorld;
            return modelWorld * global[static_cast<std::uint32_t>(p)];
        }

        float length2(const glm::vec3& v)
        {
            return glm::dot(v, v);
        }

        glm::quat quatFromMatOrtho(const glm::mat4& m)
        {
            glm::vec3       c0(m[0]);
            glm::vec3       c1(m[1]);
            const glm::vec3 c2src(m[2]);
            c0 = glm::normalize(c0);
            c1 = c1 - glm::dot(c0, c1) * c0;
            if (length2(c1) < 1e-10f)
            {
                glm::vec3 tmp = std::abs(c0.y) < 0.9f ? glm::vec3(0, 1, 0)
                                                      : glm::vec3(1, 0, 0);
                c1            = glm::normalize(glm::cross(tmp, c0));
            }
            else
                c1 = glm::normalize(c1);
            glm::vec3 c2 = glm::cross(c0, c1);
            if (glm::dot(c2, c2src) < 0.f)
            {
                c1 = -c1;
                c2 = -c2;
            }
            return glm::normalize(glm::quat_cast(glm::mat3(c0, c1, c2)));
        }

        /**
         * @brief Clamp aim direction around `forward` with yaw/pitch limits.
         *
         * Yaw about world-up×forward (right); pitch about that right.
         * Avoids quat→euler round-trips for CPU/GPU parity.
         */
        glm::vec3 clampAimDirection(glm::vec3 forward, glm::vec3 toTarget,
                                    const float maxYaw, const float maxPitch)
        {
            forward         = glm::normalize(forward);
            toTarget        = glm::normalize(toTarget);
            glm::vec3 right = glm::cross(glm::vec3(0.f, 1.f, 0.f), forward);
            if (length2(right) < 1e-8f)
                right = glm::cross(glm::vec3(1.f, 0.f, 0.f), forward);
            right                 = glm::normalize(right);
            const glm::vec3 up    = glm::cross(forward, right);
            const float     x     = glm::dot(toTarget, right);
            const float     y     = glm::dot(toTarget, up);
            const float     z     = glm::dot(toTarget, forward);
            float           yaw   = std::atan2(x, z);
            float           pitch = std::atan2(y, std::sqrt(x * x + z * z));
            yaw                   = std::clamp(yaw, -maxYaw, maxYaw);
            pitch                 = std::clamp(pitch, -maxPitch, maxPitch);
            const float     cp    = std::cos(pitch);
            const float     sp    = std::sin(pitch);
            const float     cy    = std::cos(yaw);
            const float     sy    = std::sin(yaw);
            const glm::vec3 local(sy * cp, sp, cy * cp);
            return glm::normalize(
                right * local.x + up * local.y + forward * local.z);
        }

        void setLocalRotation(LocalPose& local, const std::uint32_t joint,
                              const glm::quat& worldRot,
                              const glm::mat4& parentW, const float weight)
        {
            if (joint >= local.Size())
                return;
            const glm::quat parentQ = quatFromMatOrtho(parentW);
            const glm::quat desiredLocal =
                glm::normalize(glm::inverse(parentQ) * worldRot);
            local.joints[joint].rotation = glm::normalize(
                glm::slerp(local.joints[joint].rotation, desiredLocal, weight));
        }
    } // namespace

    std::int32_t FindRootJoint(const Skeleton& skeleton)
    {
        for (std::uint32_t i = 0; i < skeleton.JointCount(); ++i)
        {
            if (i < skeleton.parents.size() && skeleton.parents[i] < 0)
                return static_cast<std::int32_t>(i);
        }
        return skeleton.JointCount() > 0 ? 0 : -1;
    }

    bool ApplyLookAt(const Skeleton& skeleton, LocalPose& local,
                     const glm::mat4& modelWorld, const std::uint32_t joint,
                     const glm::vec3& targetWorld, const float weight,
                     const float maxYawRad, const float maxPitchRad,
                     const glm::vec3 localForward)
    {
        if (weight <= 0.f || joint >= local.Size() ||
            joint >= skeleton.JointCount())
            return false;

        const auto global   = LocalToGlobal(skeleton, local);
        const auto pw       = parentWorld(skeleton, global, joint, modelWorld);
        const auto jw       = modelWorld * global[joint];
        const auto eye      = glm::vec3(jw[3]);
        glm::vec3  toTarget = targetWorld - eye;
        if (length2(toTarget) < 1e-10f)
            return false;
        toTarget = glm::normalize(toTarget);

        const glm::quat worldRot = quatFromMatOrtho(jw);
        const glm::vec3 forward =
            glm::normalize(worldRot * glm::normalize(localForward));

        if (maxYawRad >= 0.f)
            toTarget =
                clampAimDirection(forward, toTarget, maxYawRad, maxPitchRad);
        const glm::quat delta        = quatFromTo(forward, toTarget);
        const glm::quat desiredWorld = glm::normalize(delta * worldRot);

        const float w = std::clamp(weight, 0.f, 1.f);
        setLocalRotation(local, joint, desiredWorld, pw, w);
        return true;
    }

    bool SolveTwoBoneIK(const Skeleton& skeleton, LocalPose& local,
                        const glm::mat4& modelWorld, const TwoBoneChain& chain,
                        const glm::vec3& targetWorld,
                        const glm::vec3& poleWorld, const float weight)
    {
        if (weight <= 0.f)
            return false;
        const auto n = skeleton.JointCount();
        if (chain.root >= n || chain.mid >= n || chain.tip >= n ||
            chain.root >= local.Size() || chain.mid >= local.Size() ||
            chain.tip >= local.Size())
            return false;

        auto            global = LocalToGlobal(skeleton, local);
        const glm::vec3 rootW = glm::vec3((modelWorld * global[chain.root])[3]);
        const glm::vec3 midW  = glm::vec3((modelWorld * global[chain.mid])[3]);
        const glm::vec3 tipW  = glm::vec3((modelWorld * global[chain.tip])[3]);

        const float lenA = glm::length(midW - rootW);
        const float lenB = glm::length(tipW - midW);
        if (lenA < 1e-6f || lenB < 1e-6f)
            return false;

        glm::vec3 toTarget = targetWorld - rootW;
        float     dist     = glm::length(toTarget);
        if (dist < 1e-6f)
            return false;

        const float maxReach = lenA + lenB - 1e-4f;
        const float minReach = std::abs(lenA - lenB) + 1e-4f;
        dist                 = std::clamp(dist, minReach, maxReach);
        toTarget             = glm::normalize(toTarget) * dist;

        float cosMid =
            (lenA * lenA + lenB * lenB - dist * dist) / (2.f * lenA * lenB);
        cosMid               = std::clamp(cosMid, -1.f, 1.f);
        const float midAngle = glm::pi<float>() - std::acos(cosMid);

        glm::vec3 poleDir = poleWorld - rootW;
        glm::vec3 axis    = glm::cross(toTarget, poleDir);
        if (length2(axis) < 1e-8f)
            axis = glm::cross(toTarget, glm::vec3(0.f, 1.f, 0.f));
        if (length2(axis) < 1e-8f)
            axis = glm::cross(toTarget, glm::vec3(1.f, 0.f, 0.f));
        axis = glm::normalize(axis);

        float cosRoot =
            (lenA * lenA + dist * dist - lenB * lenB) / (2.f * lenA * dist);
        cosRoot               = std::clamp(cosRoot, -1.f, 1.f);
        const float rootAngle = std::acos(cosRoot);

        const glm::vec3 unitTarget = glm::normalize(toTarget);
        const glm::vec3 bentDir =
            glm::normalize(glm::angleAxis(rootAngle, axis) * unitTarget);

        const glm::vec3 desiredMid = rootW + bentDir * lenA;
        const glm::vec3 desiredTip = rootW + unitTarget * dist;

        {
            const glm::vec3 curDir = glm::normalize(midW - rootW);
            const glm::vec3 desDir = glm::normalize(desiredMid - rootW);
            const glm::quat dQ     = quatFromTo(curDir, desDir);
            const glm::mat4 jw     = modelWorld * global[chain.root];
            const glm::quat wr     = glm::normalize(dQ * quatFromMatOrtho(jw));
            const auto      pw =
                parentWorld(skeleton, global, chain.root, modelWorld);
            setLocalRotation(local, chain.root, wr, pw,
                             std::clamp(weight, 0.f, 1.f));
        }

        global                = LocalToGlobal(skeleton, local);
        const glm::vec3 midW2 = glm::vec3((modelWorld * global[chain.mid])[3]);
        const glm::vec3 tipW2 = glm::vec3((modelWorld * global[chain.tip])[3]);
        {
            const glm::vec3 curDir = glm::normalize(tipW2 - midW2);
            const glm::vec3 desDir = glm::normalize(desiredTip - midW2);
            const glm::quat dQ     = quatFromTo(curDir, desDir);
            const glm::mat4 jw     = modelWorld * global[chain.mid];
            const glm::quat wr     = glm::normalize(dQ * quatFromMatOrtho(jw));
            const auto      pw =
                parentWorld(skeleton, global, chain.mid, modelWorld);
            setLocalRotation(local, chain.mid, wr, pw,
                             std::clamp(weight, 0.f, 1.f));
        }

        (void) midAngle;
        return true;
    }

    glm::mat4 IntegrateRootMotion(const glm::mat4&       model,
                                  const RootMotionDelta& delta,
                                  const bool             planar)
    {
        glm::vec3 t = delta.translation;
        if (planar)
            t.y = 0.f;

        glm::mat4 out = glm::translate(glm::mat4(1.f), t) * model;
        if (planar)
        {
            const glm::vec3 fwd = delta.rotation * glm::vec3(0.f, 0.f, 1.f);
            const float     yaw = std::atan2(fwd.x, fwd.z);
            if (std::abs(yaw) > 1e-5f)
            {
                const glm::vec3 pos(out[3]);
                out =
                    glm::translate(glm::mat4(1.f), pos) *
                    glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0.f, 1.f, 0.f)) *
                    glm::translate(glm::mat4(1.f), -pos) * out;
            }
        }
        return out;
    }

    void CancelRootTranslationXZ(const Skeleton& skeleton, LocalPose& local)
    {
        const auto root = FindRootJoint(skeleton);
        if (root < 0 || static_cast<std::uint32_t>(root) >= local.Size())
            return;
        auto& t = local.joints[static_cast<std::uint32_t>(root)].translation;
        t.x     = 0.f;
        t.z     = 0.f;
    }

    glm::mat4 DrivePlanarLocomotion(const glm::mat4& model,
                                    const float      metersPerSec,
                                    const float      dt,
                                    const glm::vec3  localForward)
    {
        if (metersPerSec == 0.f || dt <= 0.f)
            return model;
        const glm::vec3 worldFwd =
            glm::normalize(glm::mat3(model) * glm::normalize(localForward));
        glm::vec3 planar(worldFwd.x, 0.f, worldFwd.z);
        if (length2(planar) < 1e-10f)
            return model;
        planar = glm::normalize(planar);
        return glm::translate(glm::mat4(1.f), planar * (metersPerSec * dt)) *
               model;
    }

} // namespace FREYA_NAMESPACE
