#pragma once

#include "Freya/Config.hpp"
#include "Freya/Core/Limits.hpp"

#include <cstdint>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Per-instance vertex attributes (binding 1).
     *
     * model/prevModel drive rigid pose + TAA velocity. For skinned meshes the
     * VS also uses boneOffset into the bone matrix SSBO (prevBones for TAA).
     * materialId/entityId are consumed by GBuffer/Pick without per-draw binds.
     */
    struct InstanceTransform
    {
        glm::mat4     model      = glm::mat4(1.0f);
        glm::mat4     prevModel  = glm::mat4(1.0f);
        std::uint32_t materialId = 0;
        std::uint32_t entityId   = 0;
        std::uint32_t flags      = 0;
        std::uint32_t boneOffset = kNoSkin;
    };

} // namespace FREYA_NAMESPACE
