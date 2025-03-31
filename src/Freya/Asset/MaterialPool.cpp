#include "MaterialPool.hpp"

namespace FREYA_NAMESPACE
{
    std::uint32_t MaterialPool::CreateFromTextureFiles(
        std::vector<std::string> texturesPath)
    {
        return 0;
    }

    std::uint32_t MaterialPool::Create(std::vector<std::uint32_t> textures)
    {
        auto material = Material {
            .id = static_cast<std::uint32_t>(mMaterials.size()),
        };

        const auto samplerDescriptorSetAllocInfo =
            vk::DescriptorSetAllocateInfo()
                .setSetLayouts(mRenderPass->GetSamplerLayout())
                .setDescriptorPool(mRenderPass->GetSamplerDescriptorPool());

        material.descriptorSets =
            std::move(mDevice->Get().allocateDescriptorSets(
                samplerDescriptorSetAllocInfo));

        for (size_t binding = 0; binding < textures.size(); binding++)
        {
            auto& texture = mTexturePool->GetTexture(textures[binding]);

            auto descriptorImageInfo =
                vk::DescriptorImageInfo()
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSampler(texture.sampler)
                    .setImageView(texture.image->GetImageView());

            auto samplerDescriptorWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(material.descriptorSets[0])
                    .setDstBinding(binding)
                    .setDstArrayElement(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(descriptorImageInfo);

            mDevice->Get()
                .updateDescriptorSets(1, &samplerDescriptorWriter, 0, nullptr);
        }

        mMaterials.insert(material);

        return material.id;
    }

    void MaterialPool::Bind(std::uint32_t materialId)
    {
        if (!mMaterials.contains(materialId))
            return;

        const auto& material = mMaterials[materialId];

        mCommandPool->GetCommandBuffer().bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mRenderPass->GetPipelineLayout(),
            1,
            material.descriptorSets,
            nullptr);
    }
} // namespace FREYA_NAMESPACE