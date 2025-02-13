#include "Freya/Factories/TexturePoolFactory.hpp"

namespace FREYA_NAMESPACE
{
    Ref<TexturePool> TexturePoolFactory::CreateTexturePool()
    {
        return MakeRef<TexturePool>(mDevice, mCommandPool, mRenderPass);
    }
} // namespace FREYA_NAMESPACE
