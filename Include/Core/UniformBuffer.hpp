#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{

    struct ProjectionUniformBuffer
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 projection;
        alignas(16) glm::vec4 ambientLight;
    };

} // namespace FREYA_NAMESPACE
