#pragma once

#include "Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    class PhysicalDevice;
    class Instance;
    class Surface;
    struct QueueFamilyIndices;

    class DeviceBuilder
    {
      public:
        DeviceBuilder() :
            mDeviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME })
        {
        }

        ~DeviceBuilder() = default;

        DeviceBuilder& SetInstance(const Ref<Instance>& instance)
        {
            mInstance = instance;
            return *this;
        }

        DeviceBuilder& SetPhysicalDevice(const Ref<PhysicalDevice>& physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        DeviceBuilder& SetSurface(const Ref<Surface>& surface)
        {
            mSurface = surface;
            return *this;
        }

        Ref<Device> Build();

      protected:
        QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);

      private:
        Ref<Instance>            mInstance;
        Ref<PhysicalDevice>      mPhysicalDevice;
        Ref<Surface>             mSurface;
        std::vector<const char*> mDeviceExtensions;
    };

} // namespace FREYA_NAMESPACE
