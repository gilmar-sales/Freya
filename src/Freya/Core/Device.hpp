#pragma once

#include "PhysicalDevice.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Struct containing queue family indices for graphics, present, and
     * transfer queues.
     *
     * Used to determine queue family support for buffer sharing modes and
     * command pool creation.
     */
    struct QueueFamilyIndices
    {
        std::optional<uint32_t>
            graphicsFamily; ///< Queue family for graphics commands
        std::optional<uint32_t>
            presentFamily; ///< Queue family for presentation
        std::optional<uint32_t>
            transferFamily; ///< Queue family for transfer commands

        /**
         * @brief Checks if all required queue families are present.
         * @return true if graphicsFamily, presentFamily, and transferFamily all
         * have values
         */
        [[nodiscard]] bool isComplete() const
        {
            return graphicsFamily.has_value() && presentFamily.has_value() &&
                   transferFamily.has_value();
        }

        /**
         * @brief Checks if all queue families are unique (no sharing).
         * @return true if graphics family differs from present and transfer
         * families
         */
        [[nodiscard]] bool isUnique() const
        {
            return graphicsFamily.value() != presentFamily.value() &&
                   graphicsFamily.value() != transferFamily.value();
        }
    };

    /**
     * @brief Logical device wrapper with queue families and command submission.
     *
     * Wraps a Vulkan logical device and associated queues (graphics, present,
     * transfer). The device is destroyed in destructor. Uses QueueFamilyIndices
     * to track queue family properties for buffer/image sharing mode decisions.
     *
     * @param physicalDevice   Physical device reference
     * @param device            Logical device handle
     * @param graphicsQueue      Graphics queue handle
     * @param presentQueue       Present queue handle
     * @param transferQueue      Transfer queue handle
     * @param queueFamilyIndices Queue family indices for this device
     */
    class Device
    {
      public:
        Device(const Ref<PhysicalDevice>& physicalDevice,
               const vk::Device           device,
               const vk::Queue            graphicsQueue,
               const vk::Queue            presentQueue,
               const vk::Queue            transferQueue,
               const QueueFamilyIndices&  queueFamilyIndices) :
            mPhysicalDevice(physicalDevice), mDevice(device),
            mGraphicsQueue(graphicsQueue), mPresentQueue(presentQueue),
            mTransferQueue(transferQueue),
            mQueueFamilyIndices(queueFamilyIndices)
        {
        }

        ~Device() { mDevice.destroy(); }

        /**
         * @brief Conversion operator to check if device is valid.
         * @return true if mDevice is non-null
         */
        operator bool() const { return mDevice; }

        /**
         * @brief Returns the underlying Vulkan device handle.
         * @return Reference to vk::Device
         */
        vk::Device& Get() { return mDevice; }

        /**
         * @brief Returns the graphics queue handle.
         * @return Reference to vk::Queue
         */
        vk::Queue& GetGraphicsQueue() { return mGraphicsQueue; }

        /**
         * @brief Returns the present queue handle.
         * @return Reference to vk::Queue
         */
        vk::Queue& GetPresentQueue() { return mPresentQueue; }

        /**
         * @brief Returns the transfer queue handle.
         * @return Reference to vk::Queue
         */
        vk::Queue& GetTransferQueue() { return mTransferQueue; }

        /**
         * @brief Returns the physical device reference.
         * @return Reference to PhysicalDevice
         */
        Ref<PhysicalDevice> GetPhysicalDevice() { return mPhysicalDevice; }

        /**
         * @brief Returns the queue family indices structure.
         * @return Reference to QueueFamilyIndices
         */
        QueueFamilyIndices& GetQueueFamilyIndices()
        {
            return mQueueFamilyIndices;
        }

      private:
        Ref<PhysicalDevice> mPhysicalDevice;

        QueueFamilyIndices mQueueFamilyIndices;

        vk::Device mDevice;
        vk::Queue  mGraphicsQueue;
        vk::Queue  mPresentQueue;
        vk::Queue  mTransferQueue;
    };
} // namespace FREYA_NAMESPACE
