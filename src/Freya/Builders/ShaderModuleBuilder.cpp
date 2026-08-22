#include "Freya/Builders/ShaderModuleBuilder.hpp"

#include "Freya/Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    skr::Arc<ShaderModule> ShaderModuleBuilder::Build() const
    {
        mLogger->LogTrace("Creating shader module with file: {}",
                          mFilePath.data());

        const auto code = readFile(mFilePath);

        const auto createInfo =
            vk::ShaderModuleCreateInfo()
                .setCodeSize(code.size())
                .setPCode(reinterpret_cast<const uint32_t*>(code.data()));

        auto shaderModule = mDevice->Get().createShaderModule(createInfo);

        mLogger->Assert(shaderModule, "Failed to create shader module.");

        return skr::MakeArc<ShaderModule>(shaderModule);
    }

    std::vector<char> ShaderModuleBuilder::readFile(
        const std::string& filename) const
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        mLogger->Assert(file.is_open(), "Failed to open shader file: {}",
                        filename.data());

        const auto        fileSize = file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

} // namespace FREYA_NAMESPACE
