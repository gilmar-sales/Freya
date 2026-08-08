#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Per-instance vertex layout for mesh draws (binding 1).
     *
     * Not an application-facing type. Upload current transforms with
     * `Renderer::SetInstanceModels`; Freya fills `prevModel` for FSR.
     */
    struct InstanceTransform
    {
        glm::mat4 model     = glm::mat4(1.0f);
        glm::mat4 prevModel = glm::mat4(1.0f);
    };

} // namespace FREYA_NAMESPACE
