#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{

    struct ProjectionUniformBuffer
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 projection;
    };

} // namespace FREYA_NAMESPACE
