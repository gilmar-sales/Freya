#pragma once

#include "Core/ShaderModule.hpp"

namespace FREYA_NAMESPACE
{
    class Device;

    class ShaderModuleBuilder
    {
      public:
        ShaderModuleBuilder& SetDevice(Ref<Device> device)
        {
            mDevice = device;
            return *this;
        }

        ShaderModuleBuilder& SetFilePath(std::string filePath)
        {
            mFilePath = filePath;
            return *this;
        }

        Ref<ShaderModule> Build();

      protected:
        static std::vector<char> readFile(const std::string& filename);

      private:
        Ref<Device> mDevice;
        std::string             mFilePath;
    };
} // namespace FREYA_NAMESPACE