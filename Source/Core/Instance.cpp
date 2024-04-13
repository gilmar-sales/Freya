#include "Core/Instance.hpp"

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
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) mInstance.getProcAddr(
                "vkDestroyDebugUtilsMessengerEXT");

            if (func != nullptr)
            {
                func(mInstance, mDebugMessenger, nullptr);
            }
        }

        return vk::Result::eSuccess;
    }

} // namespace FREYA_NAMESPACE