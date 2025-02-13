#include "Freya/Builders/ShaderModuleBuilder.hpp"

#include "Freya/Core/Device.hpp"

// #include <shaderc/shaderc.hpp>

namespace FREYA_NAMESPACE
{
    Ref<ShaderModule> ShaderModuleBuilder::Build() const
    {
        const auto code = readFile(mFilePath);

        // TODO: Link shaderc correctly
        // if (not mFilePath.ends_with(".spv"))
        // {
        //     const auto compiler       = shaderc::Compiler();
        //     const auto compiledSource = compiler.CompileGlslToSpv(code.data(), code.size(), shaderc_glsl_fragment_shader, "main");
        //
        //     if (compiledSource.GetNumErrors())
        //         throw std::runtime_error("Shader compilation failed: " + compiledSource.GetErrorMessage());
        //
        //     const auto createInfo = vk::ShaderModuleCreateInfo()
        //                                 .setCode(*compiledSource.begin())
        //                                 .setCodeSize(compiledSource.end() - compiledSource.begin());
        //
        //     auto shaderModule = mDevice->Get().createShaderModule(createInfo);
        //
        //     assert(shaderModule && "Failed to create shader module.");
        //
        //     return MakeRef<ShaderModule>(shaderModule);
        // }

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