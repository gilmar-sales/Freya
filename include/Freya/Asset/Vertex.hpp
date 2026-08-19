#pragma once

#include "Freya/Config.hpp"

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
        glm::vec3  position; ///< Position in 3D space
        glm::vec3  color;    ///< RGB color
        glm::vec3  normal;   ///< Normal vector
        glm::vec3  tangent;  ///< Tangent vector
        glm::vec2  texCoord; ///< UV coordinates
        glm::uvec4 joints { 0 };
        glm::vec4  weights { 1.f, 0.f, 0.f, 0.f };
    };

} // namespace FREYA_NAMESPACE
