#pragma once

namespace FREYA_NAMESPACE
{
    class PhysicalDevice;
    class Instance;
    class Surface;
    class Device;
    struct QueueFamilyIndices;

    /**
     * @brief Builder for creating logical Device objects.
     *
     * Finds queue families, enables swapchain extension, and optionally
     * enables memory priority extensions if supported.
     *
     * @param instance      Instance reference
     * @param physicalDevice Physical device reference
     * @param surface       Surface reference (for present support)
     * @param logger        Logger reference
     */
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

        /**
         * @brief Builds and returns the Device object.
         * @return Shared pointer to created Device
         */
        Ref<Device> Build();

      protected:
        /**
         * @brief Finds queue families with graphics, present, and transfer
         * support.
         * @param device Physical device to query
         * @return QueueFamilyIndices with found queue families
         */
        QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device) const;

        /**
         * @brief Optional device extensions that may be enabled if supported.
         */
        static std::vector<const char*> OptionalExtensions;

      private:
        Ref<skr::Logger<DeviceBuilder>> mLogger;   ///< Logger reference
        Ref<Instance>                   mInstance; ///< Instance reference
        Ref<PhysicalDevice> mPhysicalDevice; ///< Physical device reference
        Ref<Surface>        mSurface;        ///< Surface reference
        std::vector<const char*>
            mDeviceExtensions; ///< Enabled device extensions
    };

} // namespace FREYA_NAMESPACE
