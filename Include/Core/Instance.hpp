#pragma once

namespace FREYA_NAMESPACE
{
    static auto ValidationLayer = "VK_LAYER_KHRONOS_validation";

    class Instance
    {
      public:
        Instance(vk::Instance instance, vk::DebugUtilsMessengerEXT debugMessenger)
            : mInstance(instance), mDebugMessenger(debugMessenger)
        {
        }

        ~Instance();

        operator bool() { return mInstance; }

        vk::Instance &Get() { return mInstance; }
        vk::DebugUtilsMessengerEXT &GetDebugMessender() { return mDebugMessenger; }

        vk::Result destroyDebugUtilsMessengerEXT();

      private:
        vk::DebugUtilsMessengerEXT mDebugMessenger;
        vk::Instance mInstance;
    };

} // namespace FREYA_NAMESPACE