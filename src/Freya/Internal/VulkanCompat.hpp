#pragma once

#include "Freya/FreyaOptions.hpp"

#include <algorithm>
#include <cstdint>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    inline vk::Extent2D ScaledExtent(const vk::Extent2D full,
                                     std::uint32_t      divisor)
    {
        divisor = std::max(1u, divisor);
        return vk::Extent2D { std::max(1u, full.width / divisor),
                              std::max(1u, full.height / divisor) };
    }

    inline vk::Extent2D ToVkExtent(const Extent2D extent)
    {
        return vk::Extent2D { extent.width, extent.height };
    }

    inline vk::ClearColorValue ToVkClearColor(const glm::vec4& color)
    {
        return vk::ClearColorValue { color.r, color.g, color.b, color.a };
    }

    inline vk::ClearValue ToVkClearValue(const glm::vec4& color)
    {
        return vk::ClearValue().setColor(ToVkClearColor(color));
    }
} // namespace FREYA_NAMESPACE
