#pragma once

#include "Freya/Core/Instance.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

namespace FREYA_NAMESPACE
{
    class Surface
    {
      public:
        Surface(const Ref<Instance>&       instance,
                const Ref<PhysicalDevice>& physicalDevice,
                const vk::SurfaceKHR       surface,
                SDL_Window*    window) :
            mInstance(instance),
            mPhysicalDevice(physicalDevice),
            mSurface(surface),
            mWindow(window)
        {
            mCapabilities = mPhysicalDevice->Get().getSurfaceCapabilitiesKHR(mSurface);
        }

        ~Surface();

        vk::SurfaceKHR& Get() { return mSurface; }

        [[nodiscard]] vk::SurfaceFormatKHR QuerySurfaceFormat() const;
        [[nodiscard]] vk::Extent2D         QueryExtent() const;
        [[nodiscard]] std::uint32_t        QueryFrameCountSupport(std::uint32_t desired) const;

      private:
        Ref<Instance>       mInstance;
        Ref<PhysicalDevice> mPhysicalDevice;
        vk::SurfaceKHR      mSurface;
        SDL_Window*                mWindow;
        vk::SurfaceCapabilitiesKHR mCapabilities;
    };

} // namespace FREYA_NAMESPACE
