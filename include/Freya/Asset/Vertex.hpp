#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Vertex format with PBR attributes + optional skinning data.
     *
     * Static meshes leave `joints` at 0 and `weights` as (1,0,0,0). Skinned
     * draws still gate on instance `boneOffset != kNoSkin`.
     */
    struct Vertex
    {
        glm::vec3 position; ///< Position in 3D space
        glm::vec3 color;    ///< RGB color
        glm::vec3 normal;   ///< Normal vector
        glm::vec3 tangent;  ///< Tangent vector
        glm::vec2 texCoord; ///< UV coordinates
        glm::uvec4 joints { 0 };
        glm::vec4  weights { 1.f, 0.f, 0.f, 0.f };

        /**
         * @brief Returns Vulkan vertex input binding descriptions.
         *
         * Binding 0: Vertex data (stride = sizeof(Vertex))
         * Binding 1: Instance data (`InstanceTransform`)
         */
        static std::vector<vk::VertexInputBindingDescription>
        GetBindingDescription();

        /**
         * @brief Returns Vulkan vertex attribute descriptions.
         *
         * Location 0-4: Vertex attributes
         * Location 5-8 / 9-12: Instance current / previous model
         * Location 13: materialId, entityId, flags, boneOffset (uvec4)
         * Location 14-15: joints / weights
         */
        static std::vector<vk::VertexInputAttributeDescription>
        GetAttributesDescription();
    };

} // namespace FREYA_NAMESPACE
