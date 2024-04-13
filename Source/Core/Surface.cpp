#include "Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    Surface::~Surface()
    {
        mInstance->Get().destroySurfaceKHR(mSurface);
    }

    vk::SurfaceFormatKHR Surface::QuerySurfaceFormat()
    {
        auto supportDetails = mPhysicalDevice->QuerySwapChainSupport(mSurface);

        for (const auto& availableFormat : supportDetails.formats)
        {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
                availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return availableFormat;
            }
        }

        return supportDetails.formats[0];
    }

    vk::Extent2D Surface::QueryExtent()
    {
        auto capabilities = mPhysicalDevice->Get().getSurfaceCapabilitiesKHR(mSurface);
        if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            auto actualExtent = vk::Extent2D()
                                    .setWidth(capabilities.maxImageExtent.width)
                                    .setHeight(capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    std::uint32_t Surface::QueryFrameCountSupport(std::uint32_t desired)
    {
        if (desired < mCapabilities.minImageCount)
        {
            desired = mCapabilities.minImageCount;
        }
        if (mCapabilities.maxImageCount > 0 && desired > mCapabilities.maxImageCount)
        {
            desired = mCapabilities.maxImageCount;
        }

        return desired;
    }
} // namespace FREYA_NAMESPACE
