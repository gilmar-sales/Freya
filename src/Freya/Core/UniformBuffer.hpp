#pragma once

#include <cstddef>
#include <cstdint>

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
    constexpr std::uint32_t MAX_LIGHTS          = 16;
    constexpr std::uint32_t MAX_SHADOW_CASCADES = 4;
    constexpr std::uint32_t MAX_SPOT_SHADOWS    = 4;
    constexpr std::uint32_t MAX_POINT_SHADOWS   = 2;

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
                          // y = intensity, z = halfHeight (area),
                          // w = castShadows (0/1)
        alignas(64) glm::vec4
            lightAreaTangents[MAX_LIGHTS]; // xyz = rect tangent (area), w
                                           // unused

        alignas(16) glm::vec4 viewPosition; // Camera position for attenuation
        alignas(16) glm::vec4
            cameraForward; ///< Camera look direction (xyz), for Deferred CSM
        // std140: lightCount + iblIntensity + exposure pack into 16 bytes
        std::uint32_t lightCount   = 0;
        float         iblIntensity = 0.7f;
        float         exposure     = 0.7f;
        float         _pad0        = 0.0f;
    };

    /**
     * @brief Push constants for the depth-only shadow pipeline.
     *
     * When `lightPosFar.w > 0`, the fragment shader writes linear
     * distance/far (Reverse-Z aware) instead of hardware NDC depth.
     * Cascade/spot passes leave `lightPosFar.w <= 0`.
     */
    struct ShadowPushConstant
    {
        glm::mat4 lightVP {};
        glm::vec4 lightPosFar {}; ///< xyz=light pos, w=far (<=0: NDC depth)
        /// x=Reverse-Z (yzw unused).
        glm::vec4 reverseZAndPad {};
    };

    /**
     * @brief Shadow sampling data (CSM + spot slots + point slots).
     *
     * Layout must match GLSL std140 (mat4 base align 16, not 64).
     * Do not use alignas(64) on mat4 members — that inserts a 32-byte hole
     * after `params` and breaks spot/point sampling.
     */
    struct alignas(256) ShadowUniformBuffer
    {
        glm::mat4 cascadeViewProj[MAX_SHADOW_CASCADES] {};
        glm::vec4 cascadeSplits {}; ///< View-space split distances
        glm::vec4
            params {}; ///< x=bias, y=normalBias, z=cascadeCount, w=softScale
        glm::mat4 spotViewProj[MAX_SPOT_SHADOWS] {};
        glm::vec4 spotLightIndex {}; ///< light indices (-1 unused)
        glm::vec4 pointLightPosFar[MAX_POINT_SHADOWS] {}; ///< xyz=pos, w=far
        glm::vec4 pointLightIndex {}; ///< light indices (-1 unused)
        glm::vec4 reverseZ {};        ///< x=1 when Reverse-Z encoding is active
        glm::vec4 pcss {}; ///< x=light size, y=max soft, z=min visibility
                           ///< (umbra floor), w=soft-shadow tap count (1–16)
    };

    static_assert(offsetof(ShadowUniformBuffer, cascadeViewProj) == 0);
    static_assert(offsetof(ShadowUniformBuffer, cascadeSplits) == 256);
    static_assert(offsetof(ShadowUniformBuffer, params) == 272);
    static_assert(offsetof(ShadowUniformBuffer, spotViewProj) == 288);
    static_assert(offsetof(ShadowUniformBuffer, spotLightIndex) == 544);
    static_assert(offsetof(ShadowUniformBuffer, pointLightPosFar) == 560);
    static_assert(offsetof(ShadowUniformBuffer, pointLightIndex) == 592);
    static_assert(offsetof(ShadowUniformBuffer, reverseZ) == 608);
    static_assert(offsetof(ShadowUniformBuffer, pcss) == 624);

    static_assert(sizeof(ProjectionUniformBuffer) % 256 == 0,
                  "ProjectionUniformBuffer must be 256-byte aligned for UBO "
                  "ring offsets");
    static_assert(sizeof(LightUniformBuffer) % 256 == 0,
                  "LightUniformBuffer must be 256-byte aligned for UBO ring "
                  "offsets");
    static_assert(sizeof(ShadowUniformBuffer) % 256 == 0,
                  "ShadowUniformBuffer must be 256-byte aligned for UBO ring "
                  "offsets");

} // namespace FREYA_NAMESPACE
