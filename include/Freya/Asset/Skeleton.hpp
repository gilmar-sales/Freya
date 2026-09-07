#pragma once

#include "Freya/Config.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Joint hierarchy + inverse-bind matrices for GPU skinning.
     */
    struct Skeleton
    {
        std::vector<std::string>  names;
        std::vector<std::int32_t> parents; ///< -1 = root
        std::vector<glm::mat4>    inverseBind;
        /// Rest-pose local TRS (Assimp node transform only — not ancestors).
        std::vector<glm::mat4> restLocal;
        /// Product of non-bone node transforms between this joint and its
        /// parent bone (or the scene root). Applied in LocalToGlobal so
        /// glTF scene-scale parents (e.g. Bulbasaur `001_0` ≈109.5) stay
        /// outside animated TRS and rest skin ≈ identity.
        std::vector<glm::mat4> nonBoneParent;

        [[nodiscard]] std::uint32_t JointCount() const
        {
            return static_cast<std::uint32_t>(names.size());
        }
    };

} // namespace FREYA_NAMESPACE
