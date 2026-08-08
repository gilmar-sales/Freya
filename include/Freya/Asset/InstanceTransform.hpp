#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Per-instance vertex attributes (binding 1).
     *
     * model/prevModel drive skinning & TAA velocity. materialId/entityId are
     * consumed by GBuffer/Pick without per-draw descriptor or push-constant
     * binds.
     */
    struct InstanceTransform
    {
        glm::mat4     model      = glm::mat4(1.0f);
        glm::mat4     prevModel  = glm::mat4(1.0f);
        std::uint32_t materialId = 0;
        std::uint32_t entityId   = 0;
        std::uint32_t flags      = 0;
        std::uint32_t _pad       = 0;
    };

} // namespace FREYA_NAMESPACE
