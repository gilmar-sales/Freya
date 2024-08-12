#pragma once

#include "Asset/TexturePool.hpp"

namespace FREYA_NAMESPACE
{

    class TexturePoolFactory
    {
      public:
        TexturePoolFactory(Ref<Device> device,
                           Ref<CommandPool>
                               commandPool) :
            mDevice(device), mCommandPool(commandPool)
        {
        }

        Ref<TexturePool> CreateTexturePool();

      private:
        Ref<Device>      mDevice;
        Ref<CommandPool> mCommandPool;
    };
} // namespace FREYA_NAMESPACE