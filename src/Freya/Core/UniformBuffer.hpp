#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Uniform buffer structure for view/projection matrices and ambient
     * light.
     *
     * Layout: alignas(64) ensures compatibility with std140/GLM.
     * Contains view matrix, projection matrix, and ambient light as vec4.
     */
    struct ProjectionUniformBuffer
    {
        alignas(64) glm::mat4 view;       ///< View matrix
        alignas(64) glm::mat4 projection; ///< Projection matrix
        alignas(64) glm::vec4
            ambientLight; ///< Ambient light direction (xyz) and intensity (w)
    };

} // namespace FREYA_NAMESPACE
