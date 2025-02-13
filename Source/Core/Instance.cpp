#include "Freya/Core/Instance.hpp"

namespace FREYA_NAMESPACE
{

    Instance::~Instance()
    {
        destroyDebugUtilsMessengerEXT();
        mInstance.destroy();
    }

    vk::Result Instance::destroyDebugUtilsMessengerEXT()
    {
        if constexpr (enableValidationLayers)
        {
            if (const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(mInstance.getProcAddr(
                    "vkDestroyDebugUtilsMessengerEXT"));
                func != nullptr)
            {
                func(mInstance, mDebugMessenger, nullptr);
            }
        }

        return vk::Result::eSuccess;
    }

} // namespace FREYA_NAMESPACE