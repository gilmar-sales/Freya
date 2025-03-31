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

        mLogger->LogTrace("Building 'fra::PhysicalDevice':");
        mLogger->LogTrace("\tName: {}", properties.deviceName.data());
        mLogger->LogTrace("\tType: {}", to_string(properties.deviceType));
        mLogger->LogTrace("\tDriver version: {}.{}.{}",
                          VK_API_VERSION_MAJOR(properties.driverVersion),
                          VK_API_VERSION_MINOR(properties.driverVersion),
                          VK_API_VERSION_PATCH(properties.driverVersion));

        auto fraPhysicalDevice = skr::MakeRef<PhysicalDevice>(physicalDevice);

        mFreyaOptions->sampleCount =
            fraPhysicalDevice->QuerySamplesSupport(mFreyaOptions->sampleCount);

        return fraPhysicalDevice;
    }

} // namespace FREYA_NAMESPACE