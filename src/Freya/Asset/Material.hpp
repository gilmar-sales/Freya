#pragma once

#include <cstdint>
#include <optional>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Parameters for creating a material.
     *
     * @param albedo     Albedo texture ID (optional)
     * @param normal     Normal map texture ID (optional)
     * @param roughness  Roughness map texture ID (optional)
     * @param emissive   Emissive texture ID (optional)
     * @param metalness  Metalness map texture ID (optional)
     */
    struct MaterialCreateInfo
    {
        std::optional<std::uint32_t> albedo;
        std::optional<std::uint32_t> normal;
        std::optional<std::uint32_t> roughness;
        std::optional<std::uint32_t> emissive;
        std::optional<std::uint32_t> metalness;
    };

    /**
     * @brief Material structure with descriptor sets.
     *
     * @param descriptorSets Vector of descriptor sets for sampler bindings
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
        std::uint32_t id;             ///< Unique material identifier
    };

} // namespace FREYA_NAMESPACE