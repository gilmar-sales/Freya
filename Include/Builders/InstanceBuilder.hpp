#pragma once

#include "Core/Instance.hpp"

namespace FREYA_NAMESPACE
{

    class InstanceBuilder
    {
      public:
        InstanceBuilder()
        {
            mApplicationName = "Freya Application";
            mEngineName      = "Freya Engine";
            mVulkanVersion   = VK_MAKE_VERSION(1, 0, 0);
            mAPIVersion      = VK_MAKE_API_VERSION(0, 1, 3, 0);
        };

        ~InstanceBuilder() = default;

        InstanceBuilder &SetApplicationName(std::string_view name);
        InstanceBuilder &SetApplicationVersion(std::uint32_t major,
                                               std::uint32_t minor,
                                               std::uint32_t patch);

        InstanceBuilder &SetEngineName();

        InstanceBuilder &SetVulkanVersion(std::uint32_t major,
                                          std::uint32_t minor,
                                          std::uint32_t patch);

        InstanceBuilder &SetAPIVersion(std::uint32_t major,
                                       std::uint32_t minor,
                                       std::uint32_t patch);

        InstanceBuilder &AddLayer(const char *layer);
        InstanceBuilder &AddLayers(std::vector<const char *> layers);
        InstanceBuilder &AddValidationLayers();

        InstanceBuilder &AddExtension(const char *extension);
        InstanceBuilder &AddExtensions(std::vector<const char *> extesions);

        Ref<Instance> Build();

      protected:
        bool checkLayerSupport(const char *layer);

      private:
        std::vector<const char *> mLayers;
        std::vector<const char *> mExtensions;

        std::string mApplicationName;
        std::uint32_t mApplicationVersion;

        std::string mEngineName;

        std::uint32_t mVulkanVersion;
        std::uint32_t mAPIVersion;
    };

} // namespace FREYA_NAMESPACE