#include "Asset/TexturePool.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <Builders/ImageBuilder.hpp>

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
        int  width, height, channels;
        auto imageData = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!imageData)
        {
            throw std::runtime_error(std::format("Failed to load texture: {}", path));
        }

        auto image = ImageBuilder(mDevice)
                         .SetUsage(ImageUsage::Texture)
                         .SetWidth(width)
                         .SetHeight(height)
                         .SetChannels(STBI_rgb_alpha)
                         .SetData(imageData)
                         .Build();

        stbi_image_free(imageData);

        return 0;
    }
} // namespace FREYA_NAMESPACE