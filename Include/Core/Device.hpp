#pragma once

#include "PhysicalDevice.hpp"
#include "Surface.hpp"

namespace FREYA_NAMESPACE
{

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> transferFamily;

        bool isComplete()
        {
            return graphicsFamily.has_value() && presentFamily.has_value() &&
                   transferFamily.has_value();
        }

        bool isUnique()
        {
            return graphicsFamily.value() != presentFamily.value() &&
                   graphicsFamily.value() != transferFamily.value();
        }
    };

    class Device
    {
      public:
        Device(Ref<PhysicalDevice> physicalDevice,
               Ref<Surface> surface,
               vk::Device device,
               vk::Queue graphicsQueue,
               vk::Queue presentQueue,
               vk::Queue transferQueue,
               QueueFamilyIndices queueFamilyIndices)
            : mPhysicalDevice(physicalDevice), mSurface(surface), mDevice(device),
              mGraphicsQueue(graphicsQueue), mPresentQueue(presentQueue),
              mTransferQueue(transferQueue), mQueueFamilyIndices(queueFamilyIndices)
        {
        }

        ~Device() { mDevice.destroy(); }

        operator bool() { return mDevice; }

        vk::Device &Get() { return mDevice; }

        vk::Queue &GetGraphicsQueue() { return mGraphicsQueue; }
        vk::Queue &GetPresentQueue() { return mPresentQueue; }
        vk::Queue &GetTransferQueue() { return mTransferQueue; }

        Ref<Surface> GetSurface() { return mSurface; }
        Ref<PhysicalDevice> GetPhysicalDevice() { return mPhysicalDevice; }

        QueueFamilyIndices &GetQueueFamilyIndices() { return mQueueFamilyIndices; }

      private:
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Surface> mSurface;

        QueueFamilyIndices mQueueFamilyIndices;

        vk::Device mDevice;
        vk::Queue mGraphicsQueue;
        vk::Queue mPresentQueue;
        vk::Queue mTransferQueue;
    };
} // namespace FREYA_NAMESPACE
