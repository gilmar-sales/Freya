#pragma once

#include "Asset/TexturePool.hpp"

namespace FREYA_NAMESPACE
{

    class TexturePoolFactory
    {
      public:
        TexturePoolFactory(const Ref<Device>&      device,
                           const Ref<CommandPool>& commandPool,
                           const Ref<ForwardPass>&  renderPass) :
            mDevice(device), mCommandPool(commandPool), mRenderPass(renderPass)
        {
        }

        Ref<TexturePool> CreateTexturePool();

      private:
        Ref<Device>      mDevice;
        Ref<CommandPool> mCommandPool;
        Ref<ForwardPass>  mRenderPass;
    };
} // namespace FREYA_NAMESPACE