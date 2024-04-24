#include "Builders/DeviceBuilder.hpp"

#include "Core/Instance.hpp"
#include "Core/PhysicalDevice.hpp"
#include "Core/Surface.hpp"

namespace FREYA_NAMESPACE
{

    std::shared_ptr<Device> DeviceBuilder::Build()
    {
        assert(mInstance.get() &&
               "Could not create an fra::Device with an invalid fra::Instance");

        assert(mPhysicalDevice.get() &&
               "Could not create an fra::Device with an invalid fra::PhysicalDevice");

        auto indices = findQueueFamilies(mPhysicalDevice->Get());

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value(),
        };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            auto queueCreateInfo =
                vk::DeviceQueueCreateInfo()
                    .setQueueFamilyIndex(queueFamily)
                    .setQueueCount(1)
                    .setPQueuePriorities(&queuePriority);

            queueCreateInfos.push_back(queueCreateInfo);
        }

        vk::PhysicalDeviceFeatures deviceFeatures {};
        deviceFeatures.depthClamp = true;

        auto createInfo =
            vk::DeviceCreateInfo()
                .setQueueCreateInfoCount(queueCreateInfos.size())
                .setPQueueCreateInfos(queueCreateInfos.data())
                .setPEnabledFeatures(&deviceFeatures)
                .setEnabledExtensionCount(mDeviceExtensions.size())
                .setPpEnabledExtensionNames(mDeviceExtensions.data());

        if (enableValidationLayers)
        {
            createInfo.setEnabledLayerCount(1).setPpEnabledLayerNames(&ValidationLayer);
        }
        else
        {
            createInfo.setEnabledLayerCount(0);
        }

        vk::Device device = mPhysicalDevice->Get().createDevice(createInfo);

        assert(device && "Could not create logical device.");

        auto graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
        auto presentQueue  = device.getQueue(indices.presentFamily.value(), 0);
        auto transferQueue = device.getQueue(indices.transferFamily.value(), 0);

        return std::make_shared<Device>(
            mPhysicalDevice,
            mSurface,
            device,
            graphicsQueue,
            presentQueue,
            transferQueue,
            indices);
    }

    QueueFamilyIndices DeviceBuilder::findQueueFamilies(vk::PhysicalDevice device)
    {
        QueueFamilyIndices indices;

        auto queueFamilies = device.getQueueFamilyProperties();

        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                indices.graphicsFamily = i;
            }

            if (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer)
            {
                indices.transferFamily = i;
            }

            assert(mSurface && "Could not create fra::Device with an invalid surface.");

            VkBool32 presentSupport = device.getSurfaceSupportKHR(i, mSurface->Get());

            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.isComplete())
            {
                break;
            }

            i++;
        }

        return indices;
    }
} // namespace FREYA_NAMESPACE
