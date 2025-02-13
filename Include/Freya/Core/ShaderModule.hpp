#pragma once

namespace FREYA_NAMESPACE
{

    class ShaderModule
    {
      public:
        ShaderModule(const vk::ShaderModule shaderModule) :
            mShaderModule(shaderModule) {}

        vk::ShaderModule& Get() { return mShaderModule; }

      private:
        vk::ShaderModule mShaderModule;
    };

} // namespace FREYA_NAMESPACE