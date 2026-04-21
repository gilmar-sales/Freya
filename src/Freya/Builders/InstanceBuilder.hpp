#pragma once

#include "Freya/Core/Instance.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Fluent builder for creating Vulkan Instance objects.
     *
     * Configures application info, layers, and extensions before building.
     * Automatically enables validation layers in debug builds and queries
     * SDL3 for required Vulkan extensions.
     *
     * @param logger Logger for build operations and assertions
     */
    class InstanceBuilder
    {
      public:
        /**
         * @brief Constructs builder, queries SDL3 Vulkan extensions.
         * @param logger Logger for build operations
         */
        InstanceBuilder(const Ref<skr::Logger<InstanceBuilder>>& logger) :
            mLogger(logger),
            mApplicationVersion(VK_MAKE_API_VERSION(0, 0, 0, 1)),
            mApplicationName("Freya Application"), mEngineName("Freya Engine"),
            mVulkanVersion(VK_MAKE_API_VERSION(0, 1, 3, 0)),
            mAPIVersion(VK_MAKE_API_VERSION(0, 1, 3, 0))
        {
            uint32_t extensionCount;
            auto     extensionNames =
                SDL_Vulkan_GetInstanceExtensions(&extensionCount);

            AddValidationLayers();

            for (auto i = 0; i < extensionCount; i++)
            {
                AddExtension(extensionNames[i]);
            }
        };

        ~InstanceBuilder() = default;

        /**
         * @brief Sets the application name.
         * @param name Application name string
         * @return Reference to this for chaining
         */
        InstanceBuilder& SetApplicationName(std::string_view name);

        /**
         * @brief Sets application version numbers.
         * @param major Major version number
         * @param minor Minor version number
         * @param patch Patch version number
         * @return Reference to this for chaining
         */
        InstanceBuilder& SetApplicationVersion(
            std::uint32_t major, std::uint32_t minor, std::uint32_t patch);

        /**
         * @brief Sets the engine name.
         * @param engineName Engine name string
         * @return Reference to this for chaining
         */
        InstanceBuilder& SetEngineName(const std::string& engineName)
        {
            mEngineName = engineName;
            return *this;
        }

        /**
         * @brief Sets the Vulkan version used for engine.
         * @param major Major version number
         * @param minor Minor version number
         * @param patch Patch version number
         * @return Reference to this for chaining
         */
        InstanceBuilder& SetVulkanVersion(const std::uint32_t major,
                                          const std::uint32_t minor,
                                          const std::uint32_t patch);

        /**
         * @brief Sets the Vulkan API version.
         * @param major Major version number
         * @param minor Minor version number
         * @param patch Patch version number
         * @return Reference to this for chaining
         */
        InstanceBuilder& SetAPIVersion(const std::uint32_t major,
                                       const std::uint32_t minor,
                                       const std::uint32_t patch);

        /**
         * @brief Adds a single validation layer.
         * @param layer Layer name to add
         * @return Reference to this for chaining
         * @note Asserts if layer is not supported
         */
        InstanceBuilder& AddLayer(const char* layer);

        /**
         * @brief Adds multiple validation layers.
         * @param layers Vector of layer names to add
         * @return Reference to this for chaining
         */
        InstanceBuilder& AddLayers(const std::vector<const char*>& layers);

        /**
         * @brief Adds Khronos validation layer if enabled.
         * @return Reference to this for chaining
         * @note Only has effect in debug builds (NDEBUG not defined)
         */
        InstanceBuilder& AddValidationLayers();

        /**
         * @brief Adds a single extension.
         * @param extension Extension name to add
         * @return Reference to this for chaining
         */
        InstanceBuilder& AddExtension(const char* extension);

        /**
         * @brief Adds multiple extensions.
         * @param extesions Vector of extension names to add
         * @return Reference to this for chaining
         */
        InstanceBuilder& AddExtensions(
            const std::vector<const char*>& extesions);

        /**
         * @brief Builds and returns the Instance object.
         * @return Shared pointer to created Instance
         * @note Creates Vulkan instance with configured layers/extensions
         */
        Ref<Instance> Build();

      protected:
        /**
         * @brief Checks if a layer is supported by the instance.
         * @param layer Layer name to check
         * @return true if layer is supported
         */
        static bool checkLayerSupport(const char* layer);

      private:
        Ref<skr::Logger<InstanceBuilder>> mLogger; ///< Logger reference

        std::vector<const char*> mLayers;     ///< Enabled validation layers
        std::vector<const char*> mExtensions; ///< Enabled extensions

        std::string   mApplicationName;    ///< Application name
        std::uint32_t mApplicationVersion; ///< Application version

        std::string mEngineName; ///< Engine name

        std::uint32_t mVulkanVersion; ///< Vulkan version for engine
        std::uint32_t mAPIVersion;    ///< Vulkan API version
    };

} // namespace FREYA_NAMESPACE