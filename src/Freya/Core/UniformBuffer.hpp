#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Uniform buffer structure for view/projection matrices and ambient
     * light.
     *
     * Layout: alignas(64) ensures compatibility with std140/GLM.
     * Contains view matrix, projection matrix, and ambient light as vec4.
     */
    struct ProjectionUniformBuffer
    {
        alignas(64) glm::mat4 view;       ///< View matrix
        alignas(64) glm::mat4 projection; ///< Projection matrix
        alignas(64) glm::vec4
            ambientLight; ///< Ambient light direction (xyz) and intensity (w)
    };

    /**
     * @brief Maximum number of lights supported by the lighting system.
     */
    constexpr std::uint32_t MAX_LIGHTS = 16;

    /**
     * @brief Light types enumeration.
     */
    enum class LightType : std::uint32_t
    {
        Point       = 0,
        Directional = 1,
        Spot        = 2
    };

    /**
     * @brief Unified light uniform buffer for forward and deferred rendering.
     *
     * Uses std140 layout with alignas(64) for GLM compatibility.
     * Supports up to MAX_LIGHTS point, directional, and spot lights.
     */
    struct LightUniformBuffer
    {
        alignas(64) glm::vec4
            lightPositions[MAX_LIGHTS]; // xyz = position, w = type (LightType)
        alignas(64) glm::vec4
            lightColorsAndRadius[MAX_LIGHTS]; // rgb = color, w = radius (for
                                              // point/spot attenuation)
        alignas(64) glm::vec4
            lightDirectionsAndCutoff[MAX_LIGHTS]; // xyz = direction, w = inner
                                                  // spotlight cutoff (cosine)
        alignas(64) glm::vec4 lightOuterCutoffAndIntensity
            [MAX_LIGHTS]; // x = outer spotlight cutoff (cosine), y = intensity

        alignas(16) glm::vec4 viewPosition; // Camera position for attenuation
        alignas(16) std::uint32_t lightCount;
        alignas(16) glm::vec3 padding; // Extra padding for std140 alignment
    };

} // namespace FREYA_NAMESPACE
