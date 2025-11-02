#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    Surface::~Surface()
    {
        mInstance->Get().destroySurfaceKHR(mSurface);
    }

    vk::SurfaceFormatKHR Surface::QuerySurfaceFormat() const
    {
        const auto supportDetails =
            mPhysicalDevice->QuerySwapChainSupport(mSurface);

        for (const auto& availableFormat : supportDetails.formats)
        {
            if (availableFormat.format == vk::Format::eB8G8R8A8Unorm &&
                availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return availableFormat;
            }
        }

        return supportDetails.formats[0];
    }

    vk::Extent2D Surface::QueryExtent() const
    {
        auto width  = mWindow->GetWidth();
        auto height = mWindow->GetHeight();

        auto capabilities =
            mPhysicalDevice->Get().getSurfaceCapabilitiesKHR(mSurface);
        if (capabilities.currentExtent.width !=
            std::numeric_limits<std::uint32_t>::max())
        {
            return vk::Extent2D()
                .setWidth(std::min(width, capabilities.currentExtent.width))
                .setHeight(std::min(height, capabilities.currentExtent.height));
        }
        else
        {
            auto actualExtent =
                vk::Extent2D()
                    .setWidth(
                        std::min(width, capabilities.maxImageExtent.width))
                    .setHeight(
                        std::min(height, capabilities.maxImageExtent.height));

            return actualExtent;
        }
    }

    std::uint32_t Surface::QueryFrameCountSupport(std::uint32_t desired) const
    {
        if (desired < mCapabilities.minImageCount)
        {
            desired = mCapabilities.minImageCount;
        }
        if (mCapabilities.maxImageCount > 0 &&
            desired > mCapabilities.maxImageCount)
        {
            desired = mCapabilities.maxImageCount;
        }

        return desired;
    }
} // namespace FREYA_NAMESPACE
