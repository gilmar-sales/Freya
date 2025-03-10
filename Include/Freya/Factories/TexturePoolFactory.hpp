#pragma once

#include "Freya/Asset/TexturePool.hpp"

namespace FREYA_NAMESPACE
{

    class TexturePoolFactory
    {
      public:
        TexturePoolFactory(const Ref<skr::ServiceProvider>& serviceProvider,
                           const Ref<Device>&               device,
                           const Ref<CommandPool>&          commandPool,
                           const Ref<ForwardPass>&          renderPass) :
            mServiceProvider(serviceProvider), mDevice(device),
            mCommandPool(commandPool), mRenderPass(renderPass)
        {
        }

        Ref<TexturePool> CreateTexturePool();

      private:
        Ref<skr::ServiceProvider> mServiceProvider;
        Ref<Device>               mDevice;
        Ref<CommandPool>          mCommandPool;
        Ref<ForwardPass>          mRenderPass;
    };
} // namespace FREYA_NAMESPACE