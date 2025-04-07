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

    TexturePool::TexturePool(const Ref<skr::ServiceProvider>& serviceProvider,
                             const Ref<Device>&               device,
                             const Ref<CommandPool>&          commandPool,
                             const Ref<RenderPass>&           renderPass) :
        mServiceProvider(serviceProvider), mDevice(device),
        mCommandPool(commandPool), mRenderPass(renderPass)
    {
        mLogger = mServiceProvider->GetService<skr::Logger<TexturePool>>();
        stbi_set_flip_vertically_on_load(true);

        const auto samplerDescriptorSetAllocInfo =
            vk::DescriptorSetAllocateInfo()
                .setSetLayouts(mRenderPass->GetSamplerLayout())
                .setDescriptorPool(mRenderPass->GetSamplerDescriptorPool());

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

        const auto stagingBuffer =
            queryStagingBuffer(width * height * STBI_rgb_alpha);

        const auto image =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Texture)
                .SetWidth(width)
                .SetHeight(height)
                .SetChannels(STBI_rgb_alpha)
                .SetStagingBuffer(stagingBuffer)
                .SetData(imageData)
                .Build();

        stbi_image_free(imageData);

        constexpr auto samplerCreateInfo =
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
                .setMaxLod(0.0f)
                .setAnisotropyEnable(true)
                .setMaxAnisotropy(16);

        const auto sampler = mDevice->Get().createSampler(samplerCreateInfo);

        const auto texture = Texture {
            .image   = image,
            .sampler = sampler,
            .width   = static_cast<std::uint32_t>(width),
            .height  = static_cast<std::uint32_t>(height),
            .id      = static_cast<std::uint32_t>(mTextures.size()),
        };

        mTextures.insert(texture);

        return texture.id;
    }

    Ref<Buffer> TexturePool::queryStagingBuffer(std::uint32_t size)
    {
        for (auto stagingBuffer : mStagingBuffers)
        {
            if (stagingBuffer->GetSize() >= size)
                return stagingBuffer;
        }

        return createStagingBuffer(size);
    }

    Ref<Buffer> TexturePool::createStagingBuffer(std::uint32_t size)
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