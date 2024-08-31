#pragma once

#include "Core/Instance.hpp"
#include "Core/PhysicalDevice.hpp"

namespace FREYA_NAMESPACE
{
    class Surface
    {
      public:
        Surface(const Ref<Instance>&       instance,
                const Ref<PhysicalDevice>& physicalDevice,
                const vk::SurfaceKHR       surface,
                const std::uint32_t        width,
                const std::uint32_t        height) :
            mInstance(instance),
            mPhysicalDevice(physicalDevice),
            mSurface(surface),
            mWidth(width),
            mHeight(height)
        {
            const auto capabilities = mPhysicalDevice->Get().getSurfaceCapabilitiesKHR(mSurface);

            mMaxExtent     = capabilities.maxImageExtent;
            mMinImageCount = capabilities.minImageCount;
            mMaxImageCount = capabilities.maxImageCount;
        }

        ~Surface();

        vk::SurfaceKHR& Get() { return mSurface; }

        [[nodiscard]] vk::SurfaceFormatKHR QuerySurfaceFormat() const;
        [[nodiscard]] vk::Extent2D         QueryExtent() const;
        [[nodiscard]] std::uint32_t        QueryFrameCountSupport(std::uint32_t desired) const;

        void SetWidth(const std::uint32_t width)
        {
            mWidth = std::min(mMaxExtent.width, width);
        }

        void SetHeight(const std::uint32_t height)
        {
            mHeight = std::min(mMaxExtent.height, height);
        }

      private:
        Ref<Instance>       mInstance;
        Ref<PhysicalDevice> mPhysicalDevice;
        vk::SurfaceKHR      mSurface;

        std::uint32_t mMinImageCount;
        std::uint32_t mMaxImageCount;
        vk::Extent2D  mMaxExtent;

        std::uint32_t mWidth;
        std::uint32_t mHeight;
    };

} // namespace FREYA_NAMESPACE
