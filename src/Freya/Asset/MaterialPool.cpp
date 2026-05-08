#include "MaterialPool.hpp"

#include "Freya/Core/Renderer.hpp"

namespace FREYA_NAMESPACE
{
    std::uint32_t MaterialPool::CreateFromTextureFiles(
        std::vector<std::string> texturesPath)
    {
        return 0;
    }

    std::uint32_t MaterialPool::Create(const MaterialCreateInfo& createInfo)
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

        // Get white fallback for albedo/normal/roughness slots
        auto& fallbackImageView = mRenderPass->GetFallbackImageView();
        auto& fallbackSampler   = mRenderPass->GetFallbackSampler();
        auto  fallbackImageInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(fallbackImageView)
                .setSampler(fallbackSampler);

        // Get black emissive fallback
        auto& emissiveFallbackImageView =
            mRenderPass->GetEmissiveFallbackImageView();
        auto& emissiveFallbackSampler =
            mRenderPass->GetEmissiveFallbackSampler();
        auto emissiveFallbackImageInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(emissiveFallbackImageView)
                .setSampler(emissiveFallbackSampler);

        // Slot 0: albedo
        {
            vk::DescriptorImageInfo imageInfo;
            if (createInfo.albedo)
            {
                auto& texture = mTexturePool->GetTexture(*createInfo.albedo);
                imageInfo =
                    vk::DescriptorImageInfo()
                        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSampler(texture.sampler)
                        .setImageView(texture.image->GetImageView());
            }
            else
            {
                imageInfo = fallbackImageInfo;
            }

            auto samplerDescriptorWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(material.descriptorSets[0])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(imageInfo);

            mDevice->Get().updateDescriptorSets(
                1, &samplerDescriptorWriter, 0, nullptr);
        }

        // Slot 1: normal
        {
            vk::DescriptorImageInfo imageInfo;
            if (createInfo.normal)
            {
                auto& texture = mTexturePool->GetTexture(*createInfo.normal);
                imageInfo =
                    vk::DescriptorImageInfo()
                        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSampler(texture.sampler)
                        .setImageView(texture.image->GetImageView());
            }
            else
            {
                imageInfo = fallbackImageInfo;
            }

            auto samplerDescriptorWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(material.descriptorSets[0])
                    .setDstBinding(1)
                    .setDstArrayElement(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(imageInfo);

            mDevice->Get().updateDescriptorSets(
                1, &samplerDescriptorWriter, 0, nullptr);
        }

        // Slot 2: roughness
        {
            vk::DescriptorImageInfo imageInfo;
            if (createInfo.roughness)
            {
                auto& texture = mTexturePool->GetTexture(*createInfo.roughness);
                imageInfo =
                    vk::DescriptorImageInfo()
                        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSampler(texture.sampler)
                        .setImageView(texture.image->GetImageView());
            }
            else
            {
                imageInfo = fallbackImageInfo;
            }

            auto samplerDescriptorWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(material.descriptorSets[0])
                    .setDstBinding(2)
                    .setDstArrayElement(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(imageInfo);

            mDevice->Get().updateDescriptorSets(
                1, &samplerDescriptorWriter, 0, nullptr);
        }

        // Slot 3: emissive
        {
            vk::DescriptorImageInfo imageInfo;
            if (createInfo.emissive)
            {
                auto& texture = mTexturePool->GetTexture(*createInfo.emissive);
                imageInfo =
                    vk::DescriptorImageInfo()
                        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSampler(texture.sampler)
                        .setImageView(texture.image->GetImageView());
            }
            else
            {
                imageInfo = emissiveFallbackImageInfo;
            }

            auto samplerDescriptorWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(material.descriptorSets[0])
                    .setDstBinding(3)
                    .setDstArrayElement(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(imageInfo);

            mDevice->Get().updateDescriptorSets(
                1, &samplerDescriptorWriter, 0, nullptr);
        }

        // Slot 4: metalness
        {
            vk::DescriptorImageInfo imageInfo;
            if (createInfo.metalness)
            {
                auto& texture = mTexturePool->GetTexture(*createInfo.metalness);
                imageInfo =
                    vk::DescriptorImageInfo()
                        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSampler(texture.sampler)
                        .setImageView(texture.image->GetImageView());
            }
            else
            {
                imageInfo = emissiveFallbackImageInfo;
            }

            auto samplerDescriptorWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(material.descriptorSets[0])
                    .setDstBinding(4)
                    .setDstArrayElement(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(imageInfo);

            mDevice->Get().updateDescriptorSets(
                1, &samplerDescriptorWriter, 0, nullptr);
        }

        mMaterials.insert(material);

        return material.id;
    }

    Material& MaterialPool::GetMaterial(uint32_t materialId)
    {
        return mMaterials[materialId];
    }
} // namespace FREYA_NAMESPACE