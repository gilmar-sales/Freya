#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/Pose.hpp"

namespace FREYA_NAMESPACE
{
    std::vector<glm::mat4> EvaluateSkeletonPose(
        const Skeleton& skeleton, const AnimationClip& clip, float timeSec)
    {
        return PoseToSkinMatrices(skeleton,
                                  SampleClip(skeleton, clip, timeSec, true));
    }

} // namespace FREYA_NAMESPACE
