#pragma once

namespace FREYA_NAMESPACE
{
    struct Material
    {
        operator std::uint32_t() const { return id; }

        std::vector<vk::DescriptorSet> descriptorSets;
        std::uint32_t                  id;
    };

} // namespace FREYA_NAMESPACE