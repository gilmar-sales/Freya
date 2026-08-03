#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Uniform buffer structure for view/projection matrices and ambient
     * light.
     *
     * alignas(256) matches common minUniformBufferOffsetAlignment so ring
     * slots can be addressed at frameIndex * sizeof(...).
     */
    struct alignas(256) ProjectionUniformBuffer
    {
        alignas(64) glm::mat4 view;       ///< View matrix
        alignas(64) glm::mat4 projection; ///< Projection matrix
        alignas(64) glm::vec4
            ambientLight; ///< Ambient light color (xyz) and intensity (w)
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
        Spot        = 2,
        Area        = 3, ///< Rectangular area light (LTC)
    };

    /**
     * @brief Unified light uniform buffer for forward and deferred rendering.
     *
     * Uses std140-friendly packing. alignas(256) ensures descriptor offsets
     * frameIndex * sizeof(LightUniformBuffer) are always valid for UBO
     * dynamic offsets across vendors.
     *
     * Area lights (type 3) reuse:
     * - position = rect center
     * - direction = rect normal
     * - outerCutoff = half-width, lightOuterCutoffAndIntensity.z = half-height
     * - lightAreaTangents = rect tangent (bitangent = cross(N, T))
     */
    struct alignas(256) LightUniformBuffer
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
            [MAX_LIGHTS]; // x = outerCutoff (spot cos) or halfWidth (area),
                          // y = intensity, z = halfHeight (area)
        alignas(64) glm::vec4
            lightAreaTangents[MAX_LIGHTS]; // xyz = rect tangent (area), w
                                           // unused

        alignas(16) glm::vec4 viewPosition; // Camera position for attenuation
        // std140: lightCount + iblIntensity + exposure pack into 16 bytes
        std::uint32_t lightCount   = 0;
        float         iblIntensity = 0.7f;
        float         exposure     = 0.7f;
        float         _pad0        = 0.0f;
    };

    static_assert(sizeof(ProjectionUniformBuffer) % 256 == 0,
                  "ProjectionUniformBuffer must be 256-byte aligned for UBO "
                  "ring offsets");
    static_assert(sizeof(LightUniformBuffer) % 256 == 0,
                  "LightUniformBuffer must be 256-byte aligned for UBO ring "
                  "offsets");

} // namespace FREYA_NAMESPACE
