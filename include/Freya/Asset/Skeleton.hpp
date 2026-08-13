#pragma once

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
        /// Rest-pose local TRS matrices (Assimp node transform).
        std::vector<glm::mat4> restLocal;

        [[nodiscard]] std::uint32_t JointCount() const
        {
            return static_cast<std::uint32_t>(names.size());
        }
    };

} // namespace FREYA_NAMESPACE
