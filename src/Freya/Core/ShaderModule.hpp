#pragma once

namespace FREYA_NAMESPACE
{
    /**
     * @brief Simple wrapper for Vulkan shader module.
     *
     * @param shaderModule Vulkan shader module handle
     */
    class ShaderModule
    {
      public:
        ShaderModule(const vk::ShaderModule shaderModule) :
            mShaderModule(shaderModule)
        {
        }

        /**
         * @brief Returns the underlying shader module handle.
         */
        vk::ShaderModule& Get() { return mShaderModule; }

      private:
        vk::ShaderModule mShaderModule;
    };

} // namespace FREYA_NAMESPACE