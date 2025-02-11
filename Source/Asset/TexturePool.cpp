#include "Asset/TexturePool.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <Builders/ImageBuilder.hpp>

namespace FREYA_NAMESPACE
{
    constexpr auto MinTextureSize = 2 * 1024 * 1024;

    TexturePool::TexturePool(const Ref<Device>&      device,
                             const Ref<CommandPool>& commandPool,
                             const Ref<ForwardPass>& renderPass) :
        mDevice(device), mCommandPool(commandPool), mRenderPass(renderPass)
    {
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
        int        width, height, channels;
        const auto imageData =
            stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!imageData)
        {
            throw std::runtime_error(
                std::format("Failed to load texture: {}", path));
        }

        const auto image =
            ImageBuilder(mDevice)
                .SetUsage(ImageUsage::Texture)
                .SetWidth(width)
                .SetHeight(height)
                .SetChannels(STBI_rgb_alpha)
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
} // namespace FREYA_NAMESPACE