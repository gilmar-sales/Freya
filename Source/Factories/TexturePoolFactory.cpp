#include "Factories/TexturePoolFactory.hpp"

namespace FREYA_NAMESPACE
{
    Ref<TexturePool> TexturePoolFactory::CreateTexturePool()
    {
        return MakeRef<TexturePool>(mDevice, mCommandPool);
    }
} // namespace FREYA_NAMESPACE