#pragma once

#include "Freya/Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    class PhysicalDevice;
    class Instance;
    class Surface;
    struct QueueFamilyIndices;

    class DeviceBuilder
    {
      public:
        DeviceBuilder(const Ref<Instance>&                   instance,
                      const Ref<PhysicalDevice>&             physicalDevice,
                      const Ref<Surface>&                    surface,
                      const Ref<skr::Logger<DeviceBuilder>>& logger) :
            mInstance(instance), mPhysicalDevice(physicalDevice),
            mSurface(surface), mLogger(logger),
            mDeviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME })
        {
        }

        Ref<Device> Build();

      protected:
        QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device) const;
        static std::vector<const char*> OptionalExtensions;

      private:
        Ref<skr::Logger<DeviceBuilder>> mLogger;
        Ref<Instance>                   mInstance;
        Ref<PhysicalDevice>             mPhysicalDevice;
        Ref<Surface>                    mSurface;
        std::vector<const char*>        mDeviceExtensions;
    };

} // namespace FREYA_NAMESPACE
