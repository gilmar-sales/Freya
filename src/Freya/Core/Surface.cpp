#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Destroys the Vulkan surface.
     */
    Surface::~Surface()
    {
        mInstance->Get().destroySurfaceKHR(mSurface);
    }

    /**
     * @brief Queries the preferred surface format.
     *
     * Prefers B8G8R8A8Unorm format with SRGB color space.
     * Falls back to first available format if preferred not found.
     *
     * @return Preferred surface format
     */
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

    /**
     * @brief Queries the surface extent based on window and device
     * capabilities.
     *
     * Returns window dimensions clamped to device capabilities.
     * If currentExtent is max uint32, uses min of window size and
     * maxImageExtent. Otherwise uses currentExtent directly.
     *
     * @return Surface extent (width/height)
     */
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

    /**
     * @brief Queries supported frame count, clamped to device limits.
     *
     * @param desired Desired frame count
     * @return Frame count clamped between minImageCount and maxImageCount (if
     * set)
     */
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
