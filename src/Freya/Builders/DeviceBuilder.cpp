#include "Freya/Builders/DeviceBuilder.hpp"

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    std::vector<const char*> DeviceBuilder::OptionalExtensions = {
        "VK_EXT_memory_priority", "VK_EXT_pageable_device_local_memory"
    };

    skr::Arc<Device> DeviceBuilder::Build()
    {
        mLogger->Assert(mInstance != nullptr,
                        "Could not create an 'fra::Device' "
                        "with an invalid 'fra::Instance'");

        mLogger->Assert(mPhysicalDevice != nullptr,
                        "Could not create an 'fra::Device' with "
                        "an invalid 'fra::PhysicalDevice'");

        auto indices = findQueueFamilies(mPhysicalDevice->Get());

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

        std::set uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value(),
            indices.transferFamily.value(),
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

        auto vulkan12Features = vk::PhysicalDeviceVulkan12Features {};
        auto features2 =
            vk::PhysicalDeviceFeatures2 {}.setPNext(&vulkan12Features);
        mPhysicalDevice->Get().getFeatures2(&features2);

        mLogger->Assert(features2.features.imageCubeArray,
                        "Physical device does not support imageCubeArray "
                        "(required for point shadow maps)");
        mLogger->Assert(features2.features.multiDrawIndirect,
                        "Physical device does not support multiDrawIndirect "
                        "(required for GPU-driven draws)");
        mLogger->Assert(
            features2.features.drawIndirectFirstInstance,
            "Physical device does not support drawIndirectFirstInstance "
            "(required for GPU-driven draws)");
        mLogger->Assert(vulkan12Features.descriptorIndexing,
                        "Physical device does not support descriptorIndexing "
                        "(required for bindless materials)");
        mLogger->Assert(
            vulkan12Features.runtimeDescriptorArray,
            "Physical device does not support runtimeDescriptorArray "
            "(required for bindless materials)");
        mLogger->Assert(
            vulkan12Features.shaderSampledImageArrayNonUniformIndexing,
            "Physical device does not support "
            "shaderSampledImageArrayNonUniformIndexing "
            "(required for bindless materials)");
        mLogger->Assert(vulkan12Features.descriptorBindingPartiallyBound,
                        "Physical device does not support "
                        "descriptorBindingPartiallyBound "
                        "(required for bindless materials)");
        mLogger->Assert(
            vulkan12Features.descriptorBindingSampledImageUpdateAfterBind,
            "Physical device does not support "
            "descriptorBindingSampledImageUpdateAfterBind "
            "(required for bindless materials)");

        auto enabled12 =
            vk::PhysicalDeviceVulkan12Features {}
                .setDescriptorIndexing(true)
                .setRuntimeDescriptorArray(true)
                .setShaderSampledImageArrayNonUniformIndexing(true)
                .setDescriptorBindingPartiallyBound(true)
                .setDescriptorBindingSampledImageUpdateAfterBind(true)
                .setDescriptorBindingStorageBufferUpdateAfterBind(true);

        auto enabledFeatures =
            vk::PhysicalDeviceFeatures()
                .setDepthClamp(features2.features.depthClamp)
                .setDepthBounds(false)
                .setSamplerAnisotropy(features2.features.samplerAnisotropy)
                .setImageCubeArray(true)
                .setMultiDrawIndirect(true)
                .setDrawIndirectFirstInstance(true);

        auto features2Enable = vk::PhysicalDeviceFeatures2 {}
                                   .setFeatures(enabledFeatures)
                                   .setPNext(&enabled12);

        // TODO: use optional extensions for memory priority
        auto optionalExtensions =
            mPhysicalDevice->FilterSupportedExtensions(OptionalExtensions);

        mDeviceExtensions.insert(mDeviceExtensions.end(),
                                 optionalExtensions.begin(),
                                 optionalExtensions.end());

        auto createInfo =
            vk::DeviceCreateInfo()
                .setPNext(&features2Enable)
                .setQueueCreateInfoCount(queueCreateInfos.size())
                .setPQueueCreateInfos(queueCreateInfos.data())
                .setEnabledExtensionCount(mDeviceExtensions.size())
                .setPpEnabledExtensionNames(mDeviceExtensions.data());

        mLogger->LogTrace("Creating logical device.");

        mLogger->LogTrace("\tEnabled Extensions: {}", mDeviceExtensions.size());
        for (const auto& extension : mDeviceExtensions)
        {
            mLogger->LogTrace("\t\t{}", extension);
        }

        vk::Device device = mPhysicalDevice->Get().createDevice(createInfo);

        mLogger->Assert(device, "Could not create logical device.");

        auto graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
        auto presentQueue  = device.getQueue(indices.presentFamily.value(), 0);
        auto transferQueue = device.getQueue(indices.transferFamily.value(), 0);

        mLogger->LogTrace("Building 'fra::Device'.");

        mLogger->LogTrace("\tGraphicsQueue: {}",
                          indices.graphicsFamily.value());
        mLogger->LogTrace("\tPresentQueue: {}", indices.presentFamily.value());
        mLogger->LogTrace("\tTransferQueue: {}",
                          indices.transferFamily.value());

        return skr::MakeArc<Device>(
            mPhysicalDevice,
            device,
            graphicsQueue,
            presentQueue,
            transferQueue,
            indices);
    }

    QueueFamilyIndices DeviceBuilder::findQueueFamilies(
        const vk::PhysicalDevice device) const
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

            mLogger->Assert(
                mSurface != nullptr,
                "Could not create fra::Device with an invalid surface.");

            VkBool32 presentSupport =
                device.getSurfaceSupportKHR(i, mSurface->Get());

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
