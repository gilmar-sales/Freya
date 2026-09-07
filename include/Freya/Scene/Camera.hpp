#pragma once

#include "Freya/Config.hpp"

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Renderer;

    /**
     * @brief App-facing camera; apply each frame via Apply(Renderer&).
     */
    struct Camera
    {
        glm::vec3 position   = glm::vec3(0.0f, 0.0f, 3.0f);
        glm::vec3 target     = glm::vec3(0.0f);
        glm::vec3 up         = glm::vec3(0.0f, 1.0f, 0.0f);
        float     fovRadians = glm::radians(60.0f);
        float     nearPlane  = 0.1f;
        float     farPlane   = 1000.0f;
        bool      useFov     = true;

        void Apply(Renderer& renderer) const;
    };

} // namespace FREYA_NAMESPACE
