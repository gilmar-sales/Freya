#pragma once

#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <cstdint>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Result of loading a skinned model file (shared skeleton).
     */
    struct SkinnedModel
    {
        std::vector<std::uint32_t> meshIds;
        Skeleton                   skeleton;
        std::vector<AnimationClip> clips;
    };

} // namespace FREYA_NAMESPACE
