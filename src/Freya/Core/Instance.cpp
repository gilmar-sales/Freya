#include "Instance.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Constructs Instance with Vulkan instance and debug messenger.
     * @param instance       Vulkan instance handle
     * @param debugMessenger Debug utils messenger handle
     */
    Instance::Instance(const vk::Instance               instance,
                       const vk::DebugUtilsMessengerEXT debugMessenger) :
        mDebugMessenger(debugMessenger), mInstance(instance)
    {
    }

    /**
     * @brief Destroys debug messenger and Vulkan instance.
     */
    Instance::~Instance()
    {
        destroyDebugUtilsMessengerEXT();
        mInstance.destroy();
    }

    /**
     * @brief Destroys the debug utils messenger if validation layers are
     * enabled.
     * @return vk::Result::eSuccess on successful destruction
     */
    vk::Result Instance::destroyDebugUtilsMessengerEXT()
    {
        if constexpr (enableValidationLayers)
        {
            if (const auto func =
                    reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                        mInstance.getProcAddr(
                            "vkDestroyDebugUtilsMessengerEXT"));
                func != nullptr)
            {
                func(mInstance, mDebugMessenger, nullptr);
            }
        }

        return vk::Result::eSuccess;
    }

} // namespace FREYA_NAMESPACE