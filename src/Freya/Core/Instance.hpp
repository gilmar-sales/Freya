#pragma once

namespace FREYA_NAMESPACE
{
    static auto ValidationLayer = "VK_LAYER_KHRONOS_validation";

    class Instance
    {
      public:
        Instance(const vk::Instance               instance,
                 const vk::DebugUtilsMessengerEXT debugMessenger);

        ~Instance();

        operator bool() const { return mInstance; }

        vk::Instance&               Get() { return mInstance; }
        vk::DebugUtilsMessengerEXT& GetDebugMessender()
        {
            return mDebugMessenger;
        }

        vk::Result destroyDebugUtilsMessengerEXT();

      private:
        vk::DebugUtilsMessengerEXT mDebugMessenger;
        vk::Instance               mInstance;
    };

} // namespace FREYA_NAMESPACE