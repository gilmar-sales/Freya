#pragma once

#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Builder for shared material descriptor resources.
     */
    class MaterialDescriptorResourcesBuilder
    {
      public:
        MaterialDescriptorResourcesBuilder(
            const skr::Arc<Device>&         device,
            const skr::Arc<PhysicalDevice>& physicalDevice,
            const skr::Arc<FreyaOptions>&   freyaOptions,
            const skr::Arc<skr::Logger<MaterialDescriptorResourcesBuilder>>&
                logger = {}) :
            mDevice(device), mPhysicalDevice(physicalDevice),
            mFreyaOptions(freyaOptions), mLogger(logger)
        {
        }

        skr::Arc<MaterialDescriptorResources> Build();

      private:
        skr::Arc<Device>         mDevice;
        skr::Arc<PhysicalDevice> mPhysicalDevice;
        skr::Arc<FreyaOptions>   mFreyaOptions;
        skr::Arc<skr::Logger<MaterialDescriptorResourcesBuilder>> mLogger;
    };
} // namespace FREYA_NAMESPACE
