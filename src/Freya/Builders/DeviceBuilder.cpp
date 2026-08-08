#include "Freya/Builders/DeviceBuilder.hpp"

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/Surface.hpp"

#if FREYA_HAS_XESS
    #include <xess/xess_vk.h>
#endif

#include <algorithm>
#include <cstring>

namespace FREYA_NAMESPACE
{
    std::vector<const char*> DeviceBuilder::OptionalExtensions = {
        "VK_EXT_memory_priority", "VK_EXT_pageable_device_local_memory"
    };

    DeviceBuilder& DeviceBuilder::ApplyXessRequirements()
    {
#if FREYA_HAS_XESS
        mApplyXessRequirements = true;

        uint32_t           extensionCount = 0;
        const char* const* extensions     = nullptr;
        const auto         status         = xessVKGetRequiredDeviceExtensions(
            mInstance->Get(), mPhysicalDevice->Get(), &extensionCount,
            &extensions);
        if (status != XESS_RESULT_SUCCESS)
        {
            mLogger->LogWarning(
                "xessVKGetRequiredDeviceExtensions failed ({}); "
                "continuing without XeSS device requirements.",
                static_cast<int>(status));
            mApplyXessRequirements = false;
            return *this;
        }

        for (uint32_t i = 0; i < extensionCount; ++i)
        {
            const char* ext = extensions[i];
            const bool  already =
                std::find_if(mDeviceExtensions.begin(), mDeviceExtensions.end(),
                             [ext](const char* e) {
                                 return std::strcmp(e, ext) == 0;
                             }) != mDeviceExtensions.end();
            if (!already)
                mDeviceExtensions.push_back(ext);
        }
#else
        mLogger->LogWarning(
            "ApplyXessRequirements ignored (FREYA_HAS_XESS=0).");
#endif
        return *this;
    }

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

        auto physicalDeviceFeatures = mPhysicalDevice->Get().getFeatures();

        mLogger->Assert(physicalDeviceFeatures.imageCubeArray,
                        "Physical device does not support imageCubeArray "
                        "(required for point shadow maps)");

        auto deviceFeatures =
            vk::PhysicalDeviceFeatures()
                .setDepthClamp(physicalDeviceFeatures.depthClamp)
                .setDepthBounds(false)
                .setSamplerAnisotropy(physicalDeviceFeatures.samplerAnisotropy)
                .setImageCubeArray(true);

        // TODO: use optional extensions for memory priority
        auto optionalExtensions =
            mPhysicalDevice->FilterSupportedExtensions(OptionalExtensions);

        mDeviceExtensions.insert(mDeviceExtensions.end(),
                                 optionalExtensions.begin(),
                                 optionalExtensions.end());

#if FREYA_HAS_XESS
        vk::PhysicalDeviceFeatures2 features2 {};
        features2.features = deviceFeatures;
        void* featureHead  = &features2;

        if (mApplyXessRequirements)
        {
            const auto featStatus = xessVKGetRequiredDeviceFeatures(
                mInstance->Get(), mPhysicalDevice->Get(), &featureHead);
            if (featStatus != XESS_RESULT_SUCCESS)
            {
                mLogger->LogWarning(
                    "xessVKGetRequiredDeviceFeatures failed ({}); "
                    "creating device without XeSS feature patch.",
                    static_cast<int>(featStatus));
                featureHead = &features2;
            }
            else
            {
                mXessFeatureChain = featureHead;
            }
        }

        auto createInfo =
            vk::DeviceCreateInfo()
                .setQueueCreateInfoCount(
                    static_cast<uint32_t>(queueCreateInfos.size()))
                .setPQueueCreateInfos(queueCreateInfos.data())
                .setEnabledExtensionCount(
                    static_cast<uint32_t>(mDeviceExtensions.size()))
                .setPpEnabledExtensionNames(mDeviceExtensions.data())
                .setPNext(featureHead);
#else
        auto createInfo =
            vk::DeviceCreateInfo()
                .setQueueCreateInfoCount(queueCreateInfos.size())
                .setPQueueCreateInfos(queueCreateInfos.data())
                .setPEnabledFeatures(&deviceFeatures)
                .setEnabledExtensionCount(mDeviceExtensions.size())
                .setPpEnabledExtensionNames(mDeviceExtensions.data());
#endif

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
