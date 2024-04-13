#pragma once

#include "Core/Instance.hpp"
#include "Core/PhysicalDevice.hpp"

namespace FREYA_NAMESPACE
{
    class Surface
    {
      public:
        Surface(std::shared_ptr<Instance> instance,
                std::shared_ptr<PhysicalDevice>
                               physicalDevice,
                vk::SurfaceKHR surface) :
            mInstance(instance),
            mPhysicalDevice(physicalDevice), mSurface(surface)
        {
            mCapabilities = mPhysicalDevice->Get().getSurfaceCapabilitiesKHR(mSurface);
        }

        ~Surface();

        vk::SurfaceKHR& Get() { return mSurface; }

        vk::SurfaceFormatKHR QuerySurfaceFormat();
        vk::Extent2D         QueryExtent();
        std::uint32_t        QueryFrameCountSupport(std::uint32_t desired);

      private:
        std::shared_ptr<Instance>       mInstance;
        std::shared_ptr<PhysicalDevice> mPhysicalDevice;

        vk::SurfaceCapabilitiesKHR mCapabilities;
        vk::SurfaceKHR             mSurface;
    };

} // namespace FREYA_NAMESPACE
