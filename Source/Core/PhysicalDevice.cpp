#include "Core/PhysicalDevice.hpp"
#include <cassert>

namespace FREYA_NAMESPACE
{

    SwapChainSupportDetails PhysicalDevice::QuerySwapChainSupport(vk::SurfaceKHR surface)
    {
        auto details = SwapChainSupportDetails {
            .capabilities = mPhysicalDevice.getSurfaceCapabilitiesKHR(surface),
            .formats      = mPhysicalDevice.getSurfaceFormatsKHR(surface),
            .presentModes = mPhysicalDevice.getSurfacePresentModesKHR(surface)
        };

        return details;
    }

    std::uint32_t PhysicalDevice::QueryCompatibleMemoryType(
        std::uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        auto memoryProperties = mPhysicalDevice.getMemoryProperties();

        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) &&
                (memoryProperties.memoryTypes[i].propertyFlags & properties) ==
                    properties)
            {
                return i;
            }
        }

        assert(!"Failed to find suitable memory type.");

        return 0;
    }

    vk::SampleCountFlagBits PhysicalDevice::QuerySamplesSupport(
        vk::SampleCountFlagBits desired)
    {
        auto deviceProperties = mPhysicalDevice.getProperties();

        auto counts = deviceProperties.limits.framebufferColorSampleCounts &
                      deviceProperties.limits.framebufferDepthSampleCounts;

        auto samples = std::vector<vk::SampleCountFlagBits> {
            vk::SampleCountFlagBits::e64, vk::SampleCountFlagBits::e32,
            vk::SampleCountFlagBits::e16, vk::SampleCountFlagBits::e8,
            vk::SampleCountFlagBits::e4,  vk::SampleCountFlagBits::e2,
        };

        for (auto& sample : samples)
        {
            if (sample & counts & desired)
            {
                return sample;
            }
        }

        if (desired != vk::SampleCountFlagBits::e1)
        {
            for (auto& sample : samples)
            {
                if (sample & counts)
                {
                    return sample;
                }
            }
        }

        return vk::SampleCountFlagBits::e1;
    }

} // namespace FREYA_NAMESPACE
