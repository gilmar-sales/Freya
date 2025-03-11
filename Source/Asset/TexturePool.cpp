#include "Freya/Asset/TexturePool.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"

namespace FREYA_NAMESPACE
{
    constexpr auto MegaBytes = 1024 * 1024;

    TexturePool::TexturePool(const Ref<skr::ServiceProvider>& serviceProvider,
                             const Ref<Device>&               device,
                             const Ref<CommandPool>&          commandPool,
                             const Ref<ForwardPass>&          renderPass) :
        mServiceProvider(serviceProvider), mDevice(device),
        mCommandPool(commandPool), mRenderPass(renderPass)
    {
        mLogger = mServiceProvider->GetService<skr::Logger<TexturePool>>();
        stbi_set_flip_vertically_on_load(true);
    }

    TexturePool::~TexturePool()
    {
        for (auto texture : mTextures)
        {
            texture.image.reset();
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
                ->SetDevice(mDevice)
                .SetUsage(ImageUsage::Texture)
                .SetWidth(width)
                .SetHeight(height)
                .SetChannels(STBI_rgb_alpha)
                .SetStagingBuffer(stagingBuffer)
                .SetData(imageData)
                .Build();

        stbi_image_free(imageData);

        const auto samplerDescriptorSetAllocInfo =
            vk::DescriptorSetAllocateInfo()
                .setSetLayouts(mRenderPass->GetSamplerLayout())
                .setDescriptorPool(mRenderPass->GetSamplerDescriptorPool());

        const auto samplerDescriptorSet = mDevice->Get().allocateDescriptorSets(
            samplerDescriptorSetAllocInfo);

        auto descriptorImageInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eReadOnlyOptimal)
                .setSampler(mRenderPass->GetSampler())
                .setImageView(image->GetImageView());

        auto samplerDescriptorWriter =
            vk::WriteDescriptorSet()
                .setDstSet(samplerDescriptorSet[0])
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(descriptorImageInfo);

        mDevice->Get()
            .updateDescriptorSets(1, &samplerDescriptorWriter, 0, nullptr);

        const auto texture = Texture {
            .image         = image,
            .descriptorSet = samplerDescriptorSet[0],
            .width         = static_cast<std::uint32_t>(width),
            .height        = static_cast<std::uint32_t>(height),
            .id            = static_cast<std::uint32_t>(mTextures.size()),
        };

        mTextures.insert(texture);

        return texture.id;
    }

    void TexturePool::Bind(const std::uint32_t uint32)
    {
        if (!mTextures.contains(uint32))
            return;

        const auto& texture = mTextures[uint32];

        const auto descriptorSets = std::array { texture.descriptorSet };

        mCommandPool->GetCommandBuffer().bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mRenderPass->GetPipelineLayout(),
            1,
            descriptorSets,
            nullptr);
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