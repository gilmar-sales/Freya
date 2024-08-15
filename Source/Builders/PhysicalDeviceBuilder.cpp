#include "Builders/PhysicalDeviceBuilder.hpp"

#include "Core/Instance.hpp"

namespace FREYA_NAMESPACE
{

    Ref<PhysicalDevice> PhysicalDeviceBuilder::Build() const
    {
        assert(mInstance != nullptr && mInstance->Get() &&
               "Could not create fra::PhysicalDevice with an invalid fra::Instance");

        static auto physicalDevices = mInstance->Get().enumeratePhysicalDevices();

        vk::PhysicalDevice physicalDevice;

        for (const auto deviceType : mPhysicalDeviceTypePriorities)
        {
            bool found = false;

            for (auto& item : physicalDevices)
            {

                if (const auto properties = item.getProperties(); deviceType == properties.deviceType)
                {
                    physicalDevice = item;
                    found          = true;
                    break;
                }
            }

            if (found)
                break;
        }

        assert(physicalDevice && "Could not select physical device.");

        return MakeRef<PhysicalDevice>(physicalDevice);
    }

} // namespace FREYA_NAMESPACE