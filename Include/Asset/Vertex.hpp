#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 color;

        static std::vector<vk::VertexInputBindingDescription> GetBindingDescription();

        static std::vector<vk::VertexInputAttributeDescription> GetAttributesDescription();
    };

} // namespace FREYA_NAMESPACE