#pragma once

#include "Core/PhysicalDevice.hpp"

namespace FREYA_NAMESPACE
{
    class Instance;

    class PhysicalDeviceBuilder
    {
      public:
        PhysicalDeviceBuilder() :
            mPhysicalDeviceTypePriorities({ vk::PhysicalDeviceType::eDiscreteGpu,
                                            vk::PhysicalDeviceType::eIntegratedGpu,
                                            vk::PhysicalDeviceType::eCpu,
                                            vk::PhysicalDeviceType::eVirtualGpu,
                                            vk::PhysicalDeviceType::eOther })
        {
        }

        PhysicalDeviceBuilder& SetInstance(const Ref<Instance>& instance)
        {
            mInstance = instance;
            return *this;
        }

        Ref<PhysicalDevice> Build() const;

      private:
        Ref<Instance>                       mInstance;
        std::vector<vk::PhysicalDeviceType> mPhysicalDeviceTypePriorities;
    };
} // namespace FREYA_NAMESPACE