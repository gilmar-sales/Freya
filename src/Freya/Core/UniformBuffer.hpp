#pragma once

#include "Freya/Core/Limits.hpp"

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    constexpr std::uint32_t MAX_LIGHTS          = kMaxLights;
    constexpr std::uint32_t MAX_SHADOW_CASCADES = kMaxShadowCascades;
    constexpr std::uint32_t MAX_SPOT_SHADOWS    = kMaxSpotShadows;
    constexpr std::uint32_t MAX_POINT_SHADOWS   = kMaxPointShadows;
    constexpr std::uint32_t MAX_MATERIAL_SETS   = kMaxMaterialSets;

    struct alignas(256) ProjectionUniformBuffer
    {
        alignas(64) glm::mat4 view;
        alignas(64) glm::mat4 projection;
        alignas(64) glm::vec4 ambientLight;
        alignas(64) glm::mat4 invViewProjection;
        alignas(64) glm::mat4 prevViewProjection;
        alignas(64) glm::mat4 unjitteredProjection;
    };

    struct alignas(256) LightUniformBuffer
    {
        alignas(64) glm::vec4 lightPositions[MAX_LIGHTS];
        alignas(64) glm::vec4 lightColorsAndRadius[MAX_LIGHTS];
        alignas(64) glm::vec4 lightDirectionsAndCutoff[MAX_LIGHTS];
        alignas(64) glm::vec4 lightOuterCutoffAndIntensity[MAX_LIGHTS];
        alignas(64) glm::vec4 lightAreaTangents[MAX_LIGHTS];

        alignas(16) glm::vec4 viewPosition;
        alignas(16) glm::vec4 cameraForward;
        std::uint32_t lightCount   = 0;
        float         iblIntensity = 0.7f;
        float         exposure     = 0.7f;
        float         _pad0        = 0.0f;
    };

    struct ShadowPushConstant
    {
        glm::mat4 lightVP {};
        glm::vec4 lightPosFar {};
        glm::vec4 reverseZAndPad {};
    };

    struct alignas(256) ShadowUniformBuffer
    {
        glm::mat4 cascadeViewProj[MAX_SHADOW_CASCADES] {};
        glm::vec4 cascadeSplits {};
        glm::vec4 params {};
        glm::mat4 spotViewProj[MAX_SPOT_SHADOWS] {};
        glm::vec4 spotLightIndex {};
        glm::vec4 pointLightPosFar[MAX_POINT_SHADOWS] {};
        glm::vec4 pointLightIndex {};
        glm::vec4 reverseZ {};
        glm::vec4 pcss {};
        glm::vec4 cascadeTexelSize {};
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
    static_assert(offsetof(ShadowUniformBuffer, cascadeTexelSize) == 640);

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
