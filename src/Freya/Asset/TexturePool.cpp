#include "Freya/Asset/TexturePool.hpp"

#ifndef NDEBUG
    #undef __OPTIMIZE__
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "Freya/Vendor/stb_image.h"

#ifndef NDEBUG
    #define __OPTIMIZE__ 1
#endif

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"

namespace FREYA_NAMESPACE
{
    constexpr auto MegaBytes = 1024 * 1024;

    TexturePool::TexturePool(
        const skr::Arc<skr::ServiceProvider>&        serviceProvider,
        const skr::Arc<Device>&                      device,
        const skr::Arc<CommandPool>&                 commandPool,
        const skr::Arc<MaterialDescriptorResources>& materials) :
        mServiceProvider(serviceProvider), mDevice(device),
        mCommandPool(commandPool), mMaterialsRes(materials)
    {
        mLogger = mServiceProvider->GetService<skr::Logger<TexturePool>>();
        stbi_set_flip_vertically_on_load(true);

        const auto samplerDescriptorSetAllocInfo =
            vk::DescriptorSetAllocateInfo()
                .setSetLayouts(mMaterialsRes->GetSamplerLayout())
                .setDescriptorPool(mMaterialsRes->GetSamplerDescriptorPool());

        mTextureDescriptorSet = mDevice->Get().allocateDescriptorSets(
            samplerDescriptorSetAllocInfo)[0];
    }

    TexturePool::~TexturePool()
    {
        for (auto texture : mTextures)
        {
            texture.image.reset();

            mDevice->Get().destroySampler(texture.sampler);
        }
    }

    std::uint32_t TexturePool::CreateTextureFromFile(std::string path)
    {
        mLogger->LogTrace("TexturePool::CreateTextureFromFile:");
        mLogger->LogTrace("\tPath: {}", path);

        int        width, height, channels;
        const auto imageData =
            stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        mLogger->Assert(imageData != nullptr, "\tFailed to load texture.");

        const auto id = CreateTextureFromMemory(
            imageData, static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height), STBI_rgb_alpha);

        stbi_image_free(imageData);
        return id;
    }

    std::uint32_t TexturePool::CreateTextureFromMemory(
        const void* pixels, std::uint32_t width, std::uint32_t height,
        std::uint32_t channels, std::uint32_t mipLevelCount)
    {
        mLogger->LogTrace("TexturePool::CreateTextureFromMemory:");
        mLogger->LogTrace("\tSize: {}x{} channels={}", width, height, channels);
        mLogger->Assert(pixels != nullptr, "\tNull pixel data.");
        mLogger->Assert(width > 0 && height > 0, "\tInvalid dimensions.");
        mLogger->Assert(channels > 0, "\tInvalid channel count.");

        const auto stagingBuffer =
            queryStagingBuffer(width * height * channels);

        auto builder =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Texture)
                .SetWidth(width)
                .SetHeight(height)
                .SetChannels(channels)
                .SetStagingBuffer(stagingBuffer)
                .SetData(const_cast<void*>(pixels));
        if (mipLevelCount > 0)
            builder.SetMipLevels(mipLevelCount);

        const auto image = builder.Build();

        const auto mipLevels = image->GetMipLevels();

        const auto samplerCreateInfo =
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
                .setUnnormalizedCoordinates(false)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMipLodBias(0.0f)
                .setMinLod(0.0f)
                .setMaxLod(static_cast<float>(mipLevels))
                .setAnisotropyEnable(true)
                .setMaxAnisotropy(16);

        const auto sampler = mDevice->Get().createSampler(samplerCreateInfo);

        const auto texture = Texture {
            .image   = image,
            .sampler = sampler,
            .width   = width,
            .height  = height,
            .id      = static_cast<std::uint32_t>(mTextures.size()),
        };

        mTextures.insert(texture);

        mMaterialsRes->WriteBindlessTexture(
            MaterialDescriptorResources::TextureHeapIndex(texture.id),
            texture.image->GetImageView(), texture.sampler);

        return texture.id;
    }

    skr::Arc<Buffer> TexturePool::queryStagingBuffer(std::uint32_t size)
    {
        for (auto stagingBuffer : mStagingBuffers)
        {
            if (stagingBuffer->GetSize() >= size)
                return stagingBuffer;
        }

        return createStagingBuffer(size);
    }

    skr::Arc<Buffer> TexturePool::createStagingBuffer(std::uint32_t size)
    {
        const auto bufferSize = (size / MegaBytes + 4) * MegaBytes;

        auto stagingBuffer =
            BufferBuilder(mDevice)
                .SetSize(bufferSize)
                .SetUsage(BufferUsage::Staging)
                .Build();

        mStagingBuffers.push_back(stagingBuffer);

        return stagingBuffer;
    }
} // namespace FREYA_NAMESPACE