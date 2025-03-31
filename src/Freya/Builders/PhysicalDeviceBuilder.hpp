#pragma once

#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class Instance;

    class PhysicalDeviceBuilder
    {
      public:
        PhysicalDeviceBuilder(
            const Ref<Instance>& instance,
            const Ref<FreyaOptions>
                                                           freyaOptions,
            const Ref<skr::Logger<PhysicalDeviceBuilder>>& logger) :
            mInstance(instance), mFreyaOptions(freyaOptions), mLogger(logger),
            mPhysicalDeviceTypePriorities(
                { vk::PhysicalDeviceType::eDiscreteGpu,
                  vk::PhysicalDeviceType::eIntegratedGpu,
                  vk::PhysicalDeviceType::eCpu,
                  vk::PhysicalDeviceType::eVirtualGpu,
                  vk::PhysicalDeviceType::eOther })
        {
        }

        Ref<PhysicalDevice> Build() const;

      private:
        Ref<Instance>     mInstance;
        Ref<FreyaOptions> mFreyaOptions;

        Ref<skr::Logger<PhysicalDeviceBuilder>> mLogger;

        std::vector<vk::PhysicalDeviceType> mPhysicalDeviceTypePriorities;
    };
} // namespace FREYA_NAMESPACE