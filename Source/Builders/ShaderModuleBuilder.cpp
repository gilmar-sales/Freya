#include "Builders/ShaderModuleBuilder.hpp"

#include "Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    Ref<ShaderModule> ShaderModuleBuilder::Build()
    {
        auto code       = readFile(mFilePath);
        auto createInfo = vk::ShaderModuleCreateInfo()
                              .setCodeSize(code.size())
                              .setPCode(reinterpret_cast<const uint32_t*>(code.data()));

        auto shaderModule = mDevice->Get().createShaderModule(createInfo);

        assert(shaderModule && "Failed to create shader module.");

        return std::make_shared<ShaderModule>(shaderModule);
    }

    std::vector<char> ShaderModuleBuilder::readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        assert(file.is_open() && "Failed to open file");

        size_t            fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

} // namespace FREYA_NAMESPACE