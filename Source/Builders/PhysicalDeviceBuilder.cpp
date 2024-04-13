#include "Builders/PhysicalDeviceBuilder.hpp"

#include "Core/Instance.hpp"

namespace FREYA_NAMESPACE
{

    std::shared_ptr<PhysicalDevice> PhysicalDeviceBuilder::Build()
    {
        assert(mInstance != nullptr && mInstance->Get() &&
               "Could not create fra::PhysicalDevice with an invalid fra::Instance");

        static auto physicalDevices = mInstance->Get().enumeratePhysicalDevices();

        vk::PhysicalDevice physicalDevice;

        for (auto deviceType : mPhysicalDeviceTypePriorities)
        {
            bool found = false;

            for (auto &item : physicalDevices)
            {
                auto properties = item.getProperties();

                if (deviceType == properties.deviceType)
                {
                    physicalDevice = item;
                    found          = true;
                    break;
                }
            }

            if (found) break;
        }

        assert(physicalDevice && "Could not select physical device.");

        return std::make_shared<PhysicalDevice>(physicalDevice);
    }

} // namespace FREYA_NAMESPACE