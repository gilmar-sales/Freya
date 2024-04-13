#pragma once

#include "Core/ShaderModule.hpp"

namespace FREYA_NAMESPACE
{
    class Device;

    class ShaderModuleBuilder
    {
      public:
        ShaderModuleBuilder &SetDevice(std::shared_ptr<Device> device)
        {
            mDevice = device;
            return *this;
        }

        ShaderModuleBuilder &SetFilePath(std::string filePath)
        {
            mFilePath = filePath;
            return *this;
        }

        std::shared_ptr<ShaderModule> Build();

      protected:
        static std::vector<char> readFile(const std::string &filename);

      private:
        std::shared_ptr<Device> mDevice;
        std::string mFilePath;
    };
} // namespace FREYA_NAMESPACE