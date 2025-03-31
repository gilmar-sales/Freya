#pragma once

#include "Freya/Core/ShaderModule.hpp"

namespace FREYA_NAMESPACE
{
    class Device;

    class ShaderModuleBuilder
    {
      public:
        ShaderModuleBuilder(const Ref<Device>&                           device,
                            const Ref<skr::Logger<ShaderModuleBuilder>>& logger,
                            const Ref<skr::ServiceProvider>& serviceProvider) :
            mDevice(device), mLogger(logger), mServiceProvider(serviceProvider)
        {
        }

        ShaderModuleBuilder& SetFilePath(const std::string& filePath)
        {
            mFilePath = filePath;
            return *this;
        }

        Ref<ShaderModule> Build() const;

      protected:
        std::vector<char> readFile(const std::string& filename) const;

      private:
        Ref<Device>                           mDevice;
        Ref<skr::Logger<ShaderModuleBuilder>> mLogger;
        Ref<skr::ServiceProvider>             mServiceProvider;
        std::string                           mFilePath;
    };
} // namespace FREYA_NAMESPACE