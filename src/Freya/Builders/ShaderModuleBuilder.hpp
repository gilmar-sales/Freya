#pragma once

#include "Freya/Core/ShaderModule.hpp"

namespace FREYA_NAMESPACE
{
    class Device;

    /**
     * @brief Builder for creating ShaderModule objects from SPIR-V files.
     *
     * Reads SPIR-V binary from file and creates shader module.
     * TODO: Integrate shaderc for GLSL compilation.
     *
     * @param device         Device reference
     * @param logger         Logger reference
     * @param serviceProvider Service provider
     */
    class ShaderModuleBuilder
    {
      public:
        ShaderModuleBuilder(const skr::Arc<Device>&                           device,
                            const skr::Arc<skr::Logger<ShaderModuleBuilder>>& logger,
                            const skr::Arc<skr::ServiceProvider>& serviceProvider) :
            mDevice(device), mLogger(logger), mServiceProvider(serviceProvider)
        {
        }

        /**
         * @brief Sets the shader file path.
         * @param filePath Path to SPIR-V shader file
         * @return Reference to this for chaining
         */
        ShaderModuleBuilder& SetFilePath(const std::string& filePath)
        {
            mFilePath = filePath;
            return *this;
        }

        /**
         * @brief Builds and returns the ShaderModule object.
         * @return Shared pointer to created ShaderModule
         */
        skr::Arc<ShaderModule> Build() const;

      protected:
        /**
         * @brief Reads binary file contents.
         * @param filename Path to file
         * @return Vector of bytes containing file data
         */
        std::vector<char> readFile(const std::string& filename) const;

      private:
        skr::Arc<Device>                           mDevice; ///< Device reference
        skr::Arc<skr::Logger<ShaderModuleBuilder>> mLogger; ///< Logger reference
        skr::Arc<skr::ServiceProvider>
                    mServiceProvider; ///< Service provider reference
        std::string mFilePath;        ///< Shader file path
    };

} // namespace FREYA_NAMESPACE