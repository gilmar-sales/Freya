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

        DeviceBuilder &SetInstance(std::shared_ptr<Instance> instance)
        {
            mInstance = instance;
            return *this;
        }

        DeviceBuilder &SetPhysicalDevice(std::shared_ptr<PhysicalDevice> physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        DeviceBuilder &SetSurface(std::shared_ptr<Surface> surface)
        {
            mSurface = surface;
            return *this;
        }

        std::shared_ptr<Device> Build();

      protected:
        QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);

      private:
        std::shared_ptr<Instance> mInstance;
        std::shared_ptr<PhysicalDevice> mPhysicalDevice;
        std::shared_ptr<Surface> mSurface;
        std::vector<const char *> mDeviceExtensions;
    };

} // namespace FREYA_NAMESPACE
