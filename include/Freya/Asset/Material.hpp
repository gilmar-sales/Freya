#pragma once

#include "Freya/Core/Buffer.hpp"

#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Surface coverage mode for materials.
     *
     * Opaque/Mask stay in the deferred MDI stream. Blend is culled into the
     * separate WBOIT translucent MDI pass (order-independent).
     */
    enum class AlphaMode : std::uint32_t
    {
        Opaque = 0,
        Mask   = 1,
        Blend  = 2,
    };

    /**
     * @brief Parameters for creating or updating a material.
     *
     * Empty texture optionals use white/black fallbacks (current look).
     * Identity factors (defaults) leave sampled values unchanged.
     *
     * @param albedo           Albedo texture ID (optional)
     * @param normal           Normal map texture ID (optional)
     * @param roughness        Roughness map texture ID (optional)
     * @param emissive         Emissive texture ID (optional)
     * @param metalness        Metalness map texture ID (optional)
     * @param albedoFactor     Multiplies sampled albedo (default white);
     *                         .a multiplies albedo alpha for Mask/Blend
     * @param roughnessFactor  Multiplies sampled roughness (default 1)
     * @param metalnessFactor  Multiplies sampled metalness (default 1;
     *                         black metalness fallback → 0)
     * @param emissiveFactor   Multiplies sampled emissive (default white)
     * @param aoFactor         Constant AO written to G-buffer (default 1)
     * @param alphaCutoff      Mask discard threshold (albedo.a * factor.a);
     *                         ignored when alphaMode is not Mask
     * @param alphaMode        Opaque, Mask (cutout), or Blend (WBOIT)
     */
    struct MaterialCreateInfo
    {
        std::optional<std::uint32_t> albedo;
        std::optional<std::uint32_t> normal;
        std::optional<std::uint32_t> roughness;
        std::optional<std::uint32_t> emissive;
        std::optional<std::uint32_t> metalness;

        glm::vec4 albedoFactor { 1.f, 1.f, 1.f, 1.f };
        float     roughnessFactor = 1.f;
        float     metalnessFactor = 1.f;
        glm::vec3 emissiveFactor { 1.f, 1.f, 1.f };
        float     aoFactor    = 1.f;
        float     alphaCutoff = 0.f;
        AlphaMode alphaMode   = AlphaMode::Opaque;
    };

    /**
     * @brief GPU std140 material factors (set 1, binding 5).
     *
     * Layout: vec4 albedo, vec4 emissive.xyz + aoFactor.w, vec2 roughMetal,
     * materialId, alphaCutoff. Size must stay 48 bytes.
     */
    struct MaterialFactorsUniform
    {
        glm::vec4 albedoFactor { 1.f, 1.f, 1.f, 1.f };
        glm::vec4 emissiveFactor { 1.f, 1.f, 1.f, 1.f }; ///< w = aoFactor
        glm::vec2 roughMetal { 1.f, 1.f };
        float     materialId  = 0.f; ///< 0–255, packed into G-buffer albedo.a
        float     alphaCutoff = 0.f; ///< 0 = cutout disabled
    };

    static_assert(sizeof(MaterialFactorsUniform) == 48,
                  "MaterialFactorsUniform must match GLSL std140 layout");

    inline MaterialFactorsUniform PackMaterialFactors(
        const MaterialCreateInfo& createInfo,
        const std::uint32_t       materialId = 0)
    {
        return MaterialFactorsUniform {
            .albedoFactor = createInfo.albedoFactor,
            .emissiveFactor =
                glm::vec4(createInfo.emissiveFactor, createInfo.aoFactor),
            .roughMetal  = { createInfo.roughnessFactor,
                             createInfo.metalnessFactor },
            .materialId  = static_cast<float>(materialId & 0xFFu),
            .alphaCutoff = createInfo.alphaCutoff,
        };
    }

    /**
     * @brief Material with descriptor sets and factor UBO.
     */
    struct Material
    {
        operator std::uint32_t() const { return id; }

        std::vector<vk::DescriptorSet>
                           descriptorSets; ///< Sampler descriptor sets
        MaterialCreateInfo createInfo;     ///< Stored create/update info
        skr::Arc<Buffer>   factorsBuffer;  ///< Factors UBO (binding 5)
        std::uint32_t      id;             ///< Unique material identifier
    };

} // namespace FREYA_NAMESPACE
