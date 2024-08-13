#pragma once

#include "Core/Instance.hpp"

namespace FREYA_NAMESPACE
{

    class InstanceBuilder
    {
      public:
        InstanceBuilder() :
            mApplicationVersion(0),
            mApplicationName("Freya Application"),
            mEngineName("Freya Engine"),
            mVulkanVersion(VK_MAKE_VERSION(1, 0, 0)),
            mAPIVersion(VK_MAKE_API_VERSION(0, 1, 3, 0)) {};

        ~InstanceBuilder() = default;

        InstanceBuilder& SetApplicationName(std::string_view name);
        InstanceBuilder& SetApplicationVersion(std::uint32_t major,
                                               std::uint32_t minor,
                                               std::uint32_t patch);

        InstanceBuilder& SetEngineName(const std::string& engineName)
        {
            mEngineName = engineName;
            return *this;
        }

        InstanceBuilder& SetVulkanVersion(const std::uint32_t major,
                                          const std::uint32_t minor,
                                          const std::uint32_t patch);

        InstanceBuilder& SetAPIVersion(const std::uint32_t major,
                                       const std::uint32_t minor,
                                       const std::uint32_t patch);

        InstanceBuilder& AddLayer(const char* layer);
        InstanceBuilder& AddLayers(const std::vector<const char*>& layers);
        InstanceBuilder& AddValidationLayers();

        InstanceBuilder& AddExtension(const char* extension);
        InstanceBuilder& AddExtensions(const std::vector<const char*>& extesions);

        Ref<Instance> Build();

      protected:
        static bool checkLayerSupport(const char* layer);

      private:
        std::vector<const char*> mLayers;
        std::vector<const char*> mExtensions;

        std::string   mApplicationName;
        std::uint32_t mApplicationVersion;

        std::string mEngineName;

        std::uint32_t mVulkanVersion;
        std::uint32_t mAPIVersion;
    };

} // namespace FREYA_NAMESPACE