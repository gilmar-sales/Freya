#include "MaterialPool.hpp"

#include "Freya/Core/Renderer.hpp"

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

        // Get white fallback info for albedo/normal/roughness slots
        auto& fallbackImageView = mRenderPass->GetFallbackImageView();
        auto& fallbackSampler   = mRenderPass->GetFallbackSampler();
        auto  fallbackImageInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(fallbackImageView)
                .setSampler(fallbackSampler);

        // Get black emissive fallback for binding 3 when no emissive texture
        auto& emissiveFallbackImageView =
            mRenderPass->GetEmissiveFallbackImageView();
        auto& emissiveFallbackSampler =
            mRenderPass->GetEmissiveFallbackSampler();
        auto emissiveFallbackImageInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(emissiveFallbackImageView)
                .setSampler(emissiveFallbackSampler);

        // Bind textures to slots 0, 1, 2, 3. Slot 3 gets black emissive if no
        // texture.
        for (size_t binding = 0; binding < 4; binding++)
        {
            vk::DescriptorImageInfo imageInfo;

            if (binding < textures.size())
            {
                auto& texture = mTexturePool->GetTexture(textures[binding]);
                imageInfo =
                    vk::DescriptorImageInfo()
                        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSampler(texture.sampler)
                        .setImageView(texture.image->GetImageView());
            }
            else if (binding == 3)
            {
                // Use black emissive fallback for missing emissive
                imageInfo = emissiveFallbackImageInfo;
            }
            else
            {
                // Use white fallback for other missing bindings
                imageInfo = fallbackImageInfo;
            }

            auto samplerDescriptorWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(material.descriptorSets[0])
                    .setDstBinding(static_cast<uint32_t>(binding))
                    .setDstArrayElement(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(imageInfo);

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
            mRenderer->GetActivePipelineLayout(),
            1,
            material.descriptorSets,
            nullptr);
    }
} // namespace FREYA_NAMESPACE