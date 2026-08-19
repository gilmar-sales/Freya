#pragma once

#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/FreyaOptions.hpp"

#include <algorithm>
#include <vector>

namespace FREYA_NAMESPACE
{
    class Instance;

    /**
     * @brief Builder for selecting and creating PhysicalDevice objects.
     *
     * Enumerates available devices and selects based on priority:
     * Discrete GPU > Integrated GPU > CPU > Virtual GPU > Other.
     * Also queries and adjusts MSAA sample count to supported maximum.
     *
     * @param instance    Instance reference
     * @param freyaOptions Freya options for sample count
     * @param logger      Logger reference
     */
    class PhysicalDeviceBuilder
    {
      public:
        PhysicalDeviceBuilder(
            const skr::Arc<Instance>& instance,
            const skr::Arc<FreyaOptions>
                                                                freyaOptions,
            const skr::Arc<skr::Logger<PhysicalDeviceBuilder>>& logger) :
            mInstance(instance), mFreyaOptions(freyaOptions), mLogger(logger),
            mPhysicalDeviceTypePriorities(
                { vk::PhysicalDeviceType::eDiscreteGpu,
                  vk::PhysicalDeviceType::eIntegratedGpu,
                  vk::PhysicalDeviceType::eCpu,
                  vk::PhysicalDeviceType::eVirtualGpu,
                  vk::PhysicalDeviceType::eOther })
        {
        }

        /**
         * @brief Prefer devices of this type first (inserts at front of the
         * priority list).
         */
        PhysicalDeviceBuilder& PreferDeviceType(vk::PhysicalDeviceType type)
        {
            mPhysicalDeviceTypePriorities.erase(
                std::remove(mPhysicalDeviceTypePriorities.begin(),
                            mPhysicalDeviceTypePriorities.end(),
                            type),
                mPhysicalDeviceTypePriorities.end());
            mPhysicalDeviceTypePriorities.insert(
                mPhysicalDeviceTypePriorities.begin(),
                type);
            return *this;
        }

        PhysicalDeviceBuilder& SetTypePriorities(
            std::vector<vk::PhysicalDeviceType> priorities)
        {
            mPhysicalDeviceTypePriorities = std::move(priorities);
            return *this;
        }

        /**
         * @brief Builds and returns the PhysicalDevice object.
         * @return Shared pointer to created PhysicalDevice
         */
        skr::Arc<PhysicalDevice> Build() const;

      private:
        skr::Arc<Instance>     mInstance;     ///< Instance reference
        skr::Arc<FreyaOptions> mFreyaOptions; ///< Freya options reference

        skr::Arc<skr::Logger<PhysicalDeviceBuilder>>
            mLogger; ///< Logger reference

        std::vector<vk::PhysicalDeviceType>
            mPhysicalDeviceTypePriorities; ///< Device type selection priority
    };
} // namespace FREYA_NAMESPACE