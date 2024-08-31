#include "Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    Surface::~Surface()
    {
        mInstance->Get().destroySurfaceKHR(mSurface);
    }

    vk::SurfaceFormatKHR Surface::QuerySurfaceFormat() const
    {
        const auto supportDetails = mPhysicalDevice->QuerySwapChainSupport(mSurface);

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

    vk::Extent2D Surface::QueryExtent() const
    {
        if (mCapabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
        {
            return mCapabilities.currentExtent;
        }

        const auto actualExtent =
            vk::Extent2D()
                .setWidth(std::min(mCapabilities.maxImageExtent.width, mWidth))
                .setHeight(std::min(mCapabilities.maxImageExtent.height, mHeight));

        return actualExtent;
    }

    std::uint32_t Surface::QueryFrameCountSupport(std::uint32_t desired) const
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
