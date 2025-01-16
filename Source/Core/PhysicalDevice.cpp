#include "Core/PhysicalDevice.hpp"
#include <cassert>

namespace FREYA_NAMESPACE
{

    SwapChainSupportDetails PhysicalDevice::QuerySwapChainSupport(const vk::SurfaceKHR surface) const
    {
        auto details = SwapChainSupportDetails {
            .capabilities = mPhysicalDevice.getSurfaceCapabilitiesKHR(surface),
            .formats      = mPhysicalDevice.getSurfaceFormatsKHR(surface),
            .presentModes = mPhysicalDevice.getSurfacePresentModesKHR(surface)
        };

        return details;
    }

    std::uint32_t PhysicalDevice::QueryCompatibleMemoryType(
        const std::uint32_t typeFilter, const vk::MemoryPropertyFlags properties) const
    {
        const auto memoryProperties = mPhysicalDevice.getMemoryProperties();

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
        const vk::SampleCountFlagBits desired) const
    {
        const auto deviceProperties = mPhysicalDevice.getProperties();

        const auto counts = deviceProperties.limits.framebufferColorSampleCounts &
                            deviceProperties.limits.framebufferDepthSampleCounts;

        const auto samples = std::vector<vk::SampleCountFlagBits> {
            vk::SampleCountFlagBits::e64,
            vk::SampleCountFlagBits::e32,
            vk::SampleCountFlagBits::e16,
            vk::SampleCountFlagBits::e8,
            vk::SampleCountFlagBits::e4,
            vk::SampleCountFlagBits::e2,
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

    vk::Format PhysicalDevice::GetDepthFormat() const
    {
        const auto candidates = std::vector {
            vk::Format::eD32SfloatS8Uint,
            vk::Format::eD32Sfloat,
            vk::Format::eD24UnormS8Uint,
            vk::Format::eD16UnormS8Uint,
            vk::Format::eD16Unorm,
        };

        constexpr auto depthFeature = vk::FormatFeatureFlagBits::eDepthStencilAttachment;
        for (auto& format : candidates)
        {

            if (auto props = mPhysicalDevice.getFormatProperties(format); (props.optimalTilingFeatures & depthFeature) == depthFeature)
            {
                return format;
            }
        }

        return vk::Format::eD32Sfloat;
    }
    std::vector<const char*> PhysicalDevice::FilterSupportedExtensions(std::vector<const char*> requestedExtensions) const
    {
        auto supportedExtensions = mPhysicalDevice.enumerateDeviceExtensionProperties();
        auto filteredExtensions  = std::vector<const char*>();
        filteredExtensions.reserve(requestedExtensions.size());

        std::ranges::copy_if(requestedExtensions, std::back_inserter(filteredExtensions), [&](const char* extension) {
            return std::ranges::find_if(supportedExtensions, [extension](vk::ExtensionProperties& extensionProperties) {
                       return extensionProperties.extensionName == extension;
                   }) != supportedExtensions.end();
        });

        return std::move(filteredExtensions);
    }
} // namespace FREYA_NAMESPACE
