#include "Asset/TexturePool.hpp"

namespace FREYA_NAMESPACE
{
    TexturePool::TexturePool(Ref<Device> device, Ref<CommandPool> commandPool) :
        mDevice(device),
        mCommandPool(commandPool)
    {
        mImageBuffers.reserve(1024);
    }

    std::uint32_t TexturePool::CreateTextureFromFile(std::string path)
    {
        return 0;
    }
} // namespace FREYA_NAMESPACE