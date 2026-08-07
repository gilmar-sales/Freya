#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Vertex format with position, color, normal, tangent, and UV.
     *
     * Provides static methods to get Vulkan binding and attribute descriptions
     * for pipeline vertex input configuration.
     *
     * @param position Vertex position (x, y, z)
     * @param color    Vertex color (r, g, b)
     * @param normal   Vertex normal (x, y, z)
     * @param tangent  Vertex tangent (x, y, z)
     * @param texCoord Texture coordinates (u, v)
     */
    struct Vertex
    {
        glm::vec3 position; ///< Position in 3D space
        glm::vec3 color;    ///< RGB color
        glm::vec3 normal;   ///< Normal vector
        glm::vec3 tangent;  ///< Tangent vector
        glm::vec2 texCoord; ///< UV coordinates

        /**
         * @brief Returns Vulkan vertex input binding descriptions.
         *
         * Binding 0: Vertex data (stride = sizeof(Vertex))
         * Binding 1: Instance data (stride = sizeof(glm::mat4))
         */
        static std::vector<vk::VertexInputBindingDescription>
        GetBindingDescription();

        /**
         * @brief Returns Vulkan vertex attribute descriptions.
         *
         * Location 0-4: Vertex attributes (position, color, normal, tangent,
         * texCoord) Location 5-8: Instance matrix columns (mat4)
         */
        static std::vector<vk::VertexInputAttributeDescription>
        GetAttributesDescription();
    };

} // namespace FREYA_NAMESPACE