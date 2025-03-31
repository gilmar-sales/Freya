#include "Freya/Builders/InstanceBuilder.hpp"

namespace FREYA_NAMESPACE
{
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT             messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*                                         pUserData)
    {
        ;
        std::cerr << "Vulkan Validation Layer ["
                  << to_string(
                         static_cast<vk::DebugUtilsMessageSeverityFlagsEXT>(
                             messageSeverity))
                  << "]: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    };

    InstanceBuilder& InstanceBuilder::SetApplicationName(
        const std::string_view name)
    {
        mApplicationName = name;
        return *this;
    }

    InstanceBuilder& InstanceBuilder::SetApplicationVersion(
        const std::uint32_t major,
        const std::uint32_t minor,
        const std::uint32_t patch)
    {
        mApplicationVersion = VK_MAKE_VERSION(major, minor, patch);
        return *this;
    }

    InstanceBuilder& InstanceBuilder::SetVulkanVersion(
        const std::uint32_t major,
        const std::uint32_t minor,
        const std::uint32_t patch)
    {
        mVulkanVersion = VK_MAKE_VERSION(major, minor, patch);
        return *this;
    }

    InstanceBuilder& InstanceBuilder::SetAPIVersion(const std::uint32_t major,
                                                    const std::uint32_t minor,
                                                    const std::uint32_t patch)
    {
        mAPIVersion = VK_MAKE_API_VERSION(0, major, minor, patch);

        return *this;
    }

    InstanceBuilder& InstanceBuilder::AddValidationLayers()
    {
        if constexpr (enableValidationLayers)
        {
            AddLayer(ValidationLayer);

            mExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return *this;
    }

    InstanceBuilder& InstanceBuilder::AddLayer(const char* layer)
    {
        assert(checkLayerSupport(layer) && layer && "not supported.");

        mLayers.push_back(layer);
        return *this;
    }

    InstanceBuilder& InstanceBuilder::AddLayers(
        const std::vector<const char*>& layers)
    {
        for (const auto layer : layers)
        {
            AddLayer(layer);
        }

        return *this;
    }

    InstanceBuilder& InstanceBuilder::AddExtension(const char* extension)
    {
        mExtensions.push_back(extension);

        return *this;
    }

    InstanceBuilder& InstanceBuilder::AddExtensions(
        const std::vector<const char*>& extesions)
    {
        for (const auto extension : extesions)
        {
            AddLayer(extension);
        }

        return *this;
    }

    Ref<Instance> InstanceBuilder::Build()
    {
        auto appInfo =
            vk::ApplicationInfo()
                .setPApplicationName(mApplicationName.c_str())
                .setApplicationVersion(mApplicationVersion)
                .setPEngineName(mEngineName.c_str())
                .setEngineVersion(mVulkanVersion)
                .setApiVersion(mAPIVersion);

        auto instanceCreateInfo =
            vk::InstanceCreateInfo().setPApplicationInfo(&appInfo);

        instanceCreateInfo.setEnabledLayerCount(mLayers.size());
        instanceCreateInfo.setPpEnabledLayerNames(mLayers.data());

        instanceCreateInfo.enabledExtensionCount   = mExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = mExtensions.data();

        auto instance = vk::createInstance(instanceCreateInfo);

        mLogger->Assert(instance != nullptr,
                        "Could not create Vulkan instance.");

        auto isValidationLayer = [](const char* layer) {
            return layer == ValidationLayer;
        };

        vk::DebugUtilsMessengerEXT debugMessenger;

        if (std::ranges::find_if(mLayers.begin(),
                                 mLayers.end(),
                                 isValidationLayer) != mLayers.end())
        {
            auto debugUtilsCreateInfo =
                vk::DebugUtilsMessengerCreateInfoEXT()
                    .setMessageSeverity(
                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
                    .setMessageType(
                        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
                    .setPfnUserCallback(DebugCallback);

            VkDebugUtilsMessengerEXT nativeDebugMessenger;

            const auto func =
                reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                    instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));

            assert(func != nullptr &&
                   "Failed to get vkCreateDebugUtilsMessengerEXT function");

            func(static_cast<VkInstance>(instance),
                 reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(
                     &debugUtilsCreateInfo),
                 nullptr,
                 &nativeDebugMessenger);

            debugMessenger = vk::DebugUtilsMessengerEXT(nativeDebugMessenger);

            mLogger->Assert(debugMessenger,
                            "Failed to set up debug messenger!");
        }

        mLogger->LogTrace("Building 'fra::Instance':");

        mLogger->LogTrace("\tApplication: {}@{}.{}.{}",
                          mApplicationName,
                          VK_API_VERSION_MAJOR(mApplicationVersion),
                          VK_API_VERSION_MINOR(mApplicationVersion),
                          VK_API_VERSION_PATCH(mApplicationVersion));

        mLogger->LogTrace("\tLayers:");
        for (auto& layer : mLayers)
        {
            mLogger->LogTrace("\t\t{}", layer);
        }

        return skr::MakeRef<Instance>(instance, debugMessenger);
    }

    bool InstanceBuilder::checkLayerSupport(const char* layer)
    {
        static auto availableLayers = vk::enumerateInstanceLayerProperties();

        bool layerFound = false;

        for (const auto& layerProperties : availableLayers)
        {
            if (std::strcmp(layer, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }

        return true;
    }

} // namespace FREYA_NAMESPACE
