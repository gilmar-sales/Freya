#include "Builders/InstanceBuilder.hpp"

namespace FREYA_NAMESPACE
{
    static VKAPI_ATTR VkBool32 VKAPI_CALL
        DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                      VkDebugUtilsMessageTypeFlagsEXT messageType,
                      const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                      void *pUserData)
    {
        std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    };

    InstanceBuilder &InstanceBuilder::SetApplicationName(std::string_view name)
    {
        mApplicationName = name;
        return *this;
    }

    InstanceBuilder &InstanceBuilder::SetApplicationVersion(std::uint32_t major,
                                                            std::uint32_t minor,
                                                            std::uint32_t patch)
    {
        mApplicationVersion = VK_MAKE_VERSION(major, minor, patch);
        return *this;
    }

    InstanceBuilder &InstanceBuilder::SetVulkanVersion(std::uint32_t major,
                                                       std::uint32_t minor,
                                                       std::uint32_t patch)
    {
        mVulkanVersion = VK_MAKE_VERSION(major, minor, patch);
        return *this;
    }

    InstanceBuilder &InstanceBuilder::AddValidationLayers()
    {
        if constexpr (enableValidationLayers)
        {
            AddLayer(ValidationLayer);

            mExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return *this;
    }

    InstanceBuilder &InstanceBuilder::AddLayer(const char *layer)
    {
        assert(checkLayerSupport(layer) && layer && "not supported.");

        mLayers.push_back(layer);
        return *this;
    }

    InstanceBuilder &InstanceBuilder::AddLayers(std::vector<const char *> layers)
    {
        for (auto layer : layers)
        {
            AddLayer(layer);
        }

        return *this;
    }

    InstanceBuilder &InstanceBuilder::AddExtension(const char *extension)
    {
        mExtensions.push_back(extension);

        return *this;
    }

    std::shared_ptr<Instance> InstanceBuilder::Build()
    {
        auto appInfo = vk::ApplicationInfo()
                           .setPApplicationName(mApplicationName.c_str())
                           .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
                           .setPEngineName(mEngineName.c_str())
                           .setEngineVersion(mVulkanVersion)
                           .setApiVersion(mAPIVersion);

        auto createInfo = vk::InstanceCreateInfo().setPApplicationInfo(&appInfo);

        createInfo.setEnabledLayerCount(mLayers.size());
        createInfo.setPpEnabledLayerNames(mLayers.data());

        createInfo.enabledExtensionCount   = mExtensions.size();
        createInfo.ppEnabledExtensionNames = mExtensions.data();

        auto instance = vk::createInstance(createInfo);
        assert(instance && "Could not create Vulkan instance.");

        auto isValidationLayer = [](const char *layer) {
            return layer == ValidationLayer;
        };

        vk::DebugUtilsMessengerEXT debugMessenger;

        if (std::find_if(mLayers.begin(), mLayers.end(), isValidationLayer) !=
            mLayers.end())
        {
            auto createInfo =
                vk::DebugUtilsMessengerCreateInfoEXT{}
                    .setMessageSeverity(
                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
                    .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
                    .setPfnUserCallback(DebugCallback);

            VkDebugUtilsMessengerEXT nativeDebugMessenger;

            auto func = (PFN_vkCreateDebugUtilsMessengerEXT) instance.getProcAddr(
                "vkCreateDebugUtilsMessengerEXT");

            assert(func != nullptr &&
                   "Failed to get vkCreateDebugUtilsMessengerEXT function");

            func((VkInstance) instance,
                 (VkDebugUtilsMessengerCreateInfoEXT *) &createInfo,
                 nullptr,
                 &nativeDebugMessenger);

            debugMessenger = vk::DebugUtilsMessengerEXT(nativeDebugMessenger);

            assert(debugMessenger && "Failed to set up debug messenger!");
        }

        return std::make_shared<Instance>(instance, debugMessenger);
    }

    bool InstanceBuilder::checkLayerSupport(const char *layer)
    {
        static auto availableLayers = vk::enumerateInstanceLayerProperties();

        bool layerFound = false;

        for (const auto &layerProperties : availableLayers)
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