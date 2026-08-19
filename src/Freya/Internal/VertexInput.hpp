#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    std::vector<vk::VertexInputBindingDescription>
    GetVertexBindingDescription();

    std::vector<vk::VertexInputAttributeDescription>
    GetVertexAttributesDescription();

    std::vector<vk::VertexInputAttributeDescription>
    GetVertexDepthAttributesDescription();
} // namespace FREYA_NAMESPACE
