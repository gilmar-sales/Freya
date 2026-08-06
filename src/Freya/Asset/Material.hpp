#pragma once

#include "Freya/Core/Buffer.hpp"

#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
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
     * @param albedoFactor     Multiplies sampled albedo (default white)
     * @param roughnessFactor  Multiplies sampled roughness (default 1)
     * @param metalnessFactor  Multiplies sampled metalness (default 1;
     *                         black metalness fallback → 0)
     * @param emissiveFactor   Multiplies sampled emissive (default white)
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
    };

    /**
     * @brief GPU std140 material factors (set 1, binding 5).
     *
     * Layout: vec4 albedo, vec4 emissive (w unused), vec2 roughMetal
     * (x=roughness, y=metalness), pad.
     */
    struct MaterialFactorsUniform
    {
        glm::vec4 albedoFactor { 1.f, 1.f, 1.f, 1.f };
        glm::vec4 emissiveFactor { 1.f, 1.f, 1.f, 0.f };
        glm::vec2 roughMetal { 1.f, 1.f };
        glm::vec2 _pad {};
    };

    static_assert(sizeof(MaterialFactorsUniform) == 48,
                  "MaterialFactorsUniform must match GLSL std140 layout");

    inline MaterialFactorsUniform PackMaterialFactors(
        const MaterialCreateInfo& createInfo)
    {
        return MaterialFactorsUniform {
            .albedoFactor   = createInfo.albedoFactor,
            .emissiveFactor = glm::vec4(createInfo.emissiveFactor, 0.f),
            .roughMetal     = { createInfo.roughnessFactor,
                                createInfo.metalnessFactor },
        };
    }

    /**
     * @brief Material with descriptor sets and factor UBO.
     *
     * @param descriptorSets Sampler + factors descriptor sets
     * @param createInfo     Last applied create/update parameters
     * @param factorsBuffer  Host-visible UBO for MaterialFactorsUniform
     * @param id             Unique material identifier
     */
    struct Material
    {
        /**
         * @brief Conversion operator to material ID.
         */
        operator std::uint32_t() const { return id; }

        std::vector<vk::DescriptorSet>
                           descriptorSets; ///< Sampler descriptor sets
        MaterialCreateInfo createInfo;     ///< Stored create/update info
        skr::Arc<Buffer>   factorsBuffer;  ///< Factors UBO (binding 5)
        std::uint32_t      id;             ///< Unique material identifier
    };

} // namespace FREYA_NAMESPACE
