#include "Builders/ShaderModuleBuilder.hpp"

#include "Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    Ref<ShaderModule> ShaderModuleBuilder::Build() const
    {
        const auto code       = readFile(mFilePath);
        const auto createInfo = vk::ShaderModuleCreateInfo()
                                    .setCodeSize(code.size())
                                    .setPCode(reinterpret_cast<const uint32_t*>(code.data()));

        auto shaderModule = mDevice->Get().createShaderModule(createInfo);

        assert(shaderModule && "Failed to create shader module.");

        return MakeRef<ShaderModule>(shaderModule);
    }

    std::vector<char> ShaderModuleBuilder::readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        assert(file.is_open() && "Failed to open file");

        const auto        fileSize = file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

} // namespace FREYA_NAMESPACE