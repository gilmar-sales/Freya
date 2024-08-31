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
        const auto capablities = mPhysicalDevice->Get().getSurfaceCapabilitiesKHR(mSurface);

        if (capablities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
        {
            return capablities.currentExtent;
        }

        const auto actualExtent =
            vk::Extent2D()
                .setWidth(std::min(mMaxExtent.width, mWidth))
                .setHeight(std::min(mMaxExtent.height, mHeight));

        return actualExtent;
    }

    std::uint32_t Surface::QueryFrameCountSupport(const std::uint32_t desired) const
    {
        return std::clamp(desired, mMinImageCount, mMaxImageCount);
    }
} // namespace FREYA_NAMESPACE
