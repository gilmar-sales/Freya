#pragma once

#include "Freya/Core/Instance.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Window.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Wraps a Vulkan surface and queries surface properties.
     *
     * Creates and manages a Vulkan surface from SDL3 window. Queries
     * capabilities on construction and provides methods to get preferred format
     * and extent.
     *
     * @param instance       Vulkan instance
     * @param physicalDevice Physical device for capability queries
     * @param window         SDL3 window handle
     * @param surface        Vulkan surface handle
     */
    class Surface
    {
      public:
        Surface(const Ref<Instance>&       instance,
                const Ref<PhysicalDevice>& physicalDevice,
                const Ref<Window>&         window,
                const vk::SurfaceKHR       surface) :
            mInstance(instance), mPhysicalDevice(physicalDevice),
            mSurface(surface), mWindow(window)
        {
            mCapabilities =
                mPhysicalDevice->Get().getSurfaceCapabilitiesKHR(mSurface);
        }

        ~Surface();

        /**
         * @brief Returns the underlying Vulkan surface handle.
         * @return Reference to vk::SurfaceKHR
         */
        vk::SurfaceKHR& Get() { return mSurface; }

        /**
         * @brief Queries the preferred surface format (B8G8R8A8Unorm if
         * available).
         * @return vk::SurfaceFormatKHR with preferred format and color space
         */
        [[nodiscard]] vk::SurfaceFormatKHR QuerySurfaceFormat() const;

        /**
         * @brief Queries the surface extent based on window size and device
         * capabilities.
         * @return vk::Extent2D with width/height clamped to device capabilities
         */
        [[nodiscard]] vk::Extent2D QueryExtent() const;

        /**
         * @brief Queries supported frame count, clamped between min and max
         * image counts.
         * @param desired Desired frame count
         * @return Supported frame count within device limits
         */
        [[nodiscard]] std::uint32_t QueryFrameCountSupport(
            std::uint32_t desired) const;

      private:
        Ref<Instance>              mInstance;
        Ref<PhysicalDevice>        mPhysicalDevice;
        Ref<Window>                mWindow;
        vk::SurfaceKHR             mSurface;
        vk::SurfaceCapabilitiesKHR mCapabilities;
    };

} // namespace FREYA_NAMESPACE
