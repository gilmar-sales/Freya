#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 color;

        static vk::VertexInputBindingDescription GetBindingDescription();

        static std::array<vk::VertexInputAttributeDescription, 2>
            GetAttributesDescription();
    };

} // namespace FREYA_NAMESPACE