#pragma once

#include "Freya/Core/ShaderModule.hpp"

namespace FREYA_NAMESPACE
{
    class Device;

    class ShaderModuleBuilder
    {
      public:
        ShaderModuleBuilder& SetDevice(const Ref<Device>& device)
        {
            mDevice = device;
            return *this;
        }

        ShaderModuleBuilder& SetFilePath(const std::string& filePath)
        {
            mFilePath = filePath;
            return *this;
        }

        Ref<ShaderModule> Build() const;

      protected:
        static std::vector<char> readFile(const std::string& filename);

      private:
        Ref<Device> mDevice;
        std::string mFilePath;
    };
} // namespace FREYA_NAMESPACE