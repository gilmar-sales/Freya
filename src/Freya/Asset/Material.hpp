#pragma once

namespace FREYA_NAMESPACE
{
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