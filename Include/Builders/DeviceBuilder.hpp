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
        DeviceBuilder()
            : mInstance(nullptr), mPhysicalDevice(nullptr), mSurface(nullptr),
              mDeviceExtensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME})
        {
        }

        ~DeviceBuilder() = default;

        DeviceBuilder &SetInstance(Ref<Instance> instance)
        {
            mInstance = instance;
            return *this;
        }

        DeviceBuilder &SetPhysicalDevice(Ref<PhysicalDevice> physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        DeviceBuilder &SetSurface(Ref<Surface> surface)
        {
            mSurface = surface;
            return *this;
        }

        Ref<Device> Build();

      protected:
        QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);

      private:
        Ref<Instance> mInstance;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Surface> mSurface;
        std::vector<const char *> mDeviceExtensions;
    };

} // namespace FREYA_NAMESPACE
