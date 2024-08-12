#include "Factories/TexturePoolFactory.hpp"

namespace FREYA_NAMESPACE
{
    Ref<TexturePool> TexturePoolFactory::CreateTexturePool()
    {
        return std::make_shared<TexturePool>(mDevice, mCommandPool);
    }
} // namespace FREYA_NAMESPACE