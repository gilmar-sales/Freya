#include "Freya/Builders/PhysicalDeviceBuilder.hpp"

#include "Freya/Core/Instance.hpp"

namespace FREYA_NAMESPACE
{

    Ref<PhysicalDevice> PhysicalDeviceBuilder::Build() const
    {
        mLogger->Assert(mInstance != nullptr && mInstance->Get(),
                        "Could not build 'fra::PhysicalDevice' with an invalid "
                        "fra::Instance");

        static auto physicalDevices =
            mInstance->Get().enumeratePhysicalDevices();

        vk::PhysicalDevice physicalDevice;

        for (const auto deviceType : mPhysicalDeviceTypePriorities)
        {
            bool found = false;

            for (auto& item : physicalDevices)
            {

                if (const auto properties = item.getProperties();
                    deviceType == properties.deviceType)
                {
                    physicalDevice = item;
                    found          = true;
                    break;
                }
            }

            if (found)
                break;
        }

        mLogger->Assert(physicalDevice, "Could not select physical device.");

        auto properties = physicalDevice.getProperties();
        auto deviceName = properties.deviceName.data();
        auto deviceType = to_string(properties.deviceType);

        auto driverVMajor = VK_API_VERSION_MAJOR(properties.driverVersion);
        auto driverVMinor = VK_API_VERSION_MINOR(properties.driverVersion);
        auto driverVPatch = VK_API_VERSION_PATCH(properties.driverVersion);

        mLogger->LogTrace("Building 'fra::PhysicalDevice':");
        mLogger->LogTrace("\tName: {}", deviceName);
        mLogger->LogTrace("\tType: {}", deviceType);
        mLogger->LogTrace("\tDriver version: {}.{}.{}",
                          driverVMajor,
                          driverVMinor,
                          driverVPatch);

        return MakeRef<PhysicalDevice>(physicalDevice);
    }

} // namespace FREYA_NAMESPACE