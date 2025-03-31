#pragma once

#include "Freya/Core/Instance.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Window.hpp"

namespace FREYA_NAMESPACE
{
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

        vk::SurfaceKHR& Get() { return mSurface; }

        [[nodiscard]] vk::SurfaceFormatKHR QuerySurfaceFormat() const;
        [[nodiscard]] vk::Extent2D         QueryExtent() const;
        [[nodiscard]] std::uint32_t        QueryFrameCountSupport(
                   std::uint32_t desired) const;

      private:
        Ref<Instance>              mInstance;
        Ref<PhysicalDevice>        mPhysicalDevice;
        Ref<Window>                mWindow;
        vk::SurfaceKHR             mSurface;
        vk::SurfaceCapabilitiesKHR mCapabilities;
    };

} // namespace FREYA_NAMESPACE
