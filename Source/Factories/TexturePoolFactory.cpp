#include "Freya/Factories/TexturePoolFactory.hpp"

namespace FREYA_NAMESPACE
{
    Ref<TexturePool> TexturePoolFactory::CreateTexturePool()
    {
        return MakeRef<TexturePool>(
            mServiceProvider, mDevice, mCommandPool, mRenderPass);
    }
} // namespace FREYA_NAMESPACE
