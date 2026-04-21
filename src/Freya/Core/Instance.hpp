#pragma once

namespace FREYA_NAMESPACE
{
    static auto ValidationLayer = "VK_LAYER_KHRONOS_validation";

    /**
     * @brief Wraps a Vulkan instance and debug messenger for lifecycle
     * management.
     *
     * Manages the Vulkan instance and debug utilities messenger. The destructor
     * properly cleans up the debug messenger if validation layers are enabled.
     *
     * @param instance       Raw Vulkan instance handle
     * @param debugMessenger Debug utils messenger for validation layer
     * callbacks
     */
    class Instance
    {
      public:
        Instance(const vk::Instance               instance,
                 const vk::DebugUtilsMessengerEXT debugMessenger);

        ~Instance();

        /**
         * @brief Conversion operator to check if instance is valid.
         * @return true if mInstance is non-null
         */
        operator bool() const { return mInstance; }

        /**
         * @brief Returns the underlying Vulkan instance handle.
         * @return Reference to vk::Instance
         */
        vk::Instance& Get() { return mInstance; }

        /**
         * @brief Returns the debug messenger handle.
         * @return Reference to vk::DebugUtilsMessengerEXT
         */
        vk::DebugUtilsMessengerEXT& GetDebugMessender()
        {
            return mDebugMessenger;
        }

        /**
         * @brief Destroys the debug utils messenger if validation layers are
         * enabled.
         * @return vk::Result::eSuccess on successful destruction
         */
        vk::Result destroy_debug_utilsMessengerEXT();

      private:
        vk::DebugUtilsMessengerEXT mDebugMessenger;
        vk::Instance               mInstance;
    };

} // namespace FREYA_NAMESPACE