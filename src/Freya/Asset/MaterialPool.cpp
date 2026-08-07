#include "MaterialPool.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/Renderer.hpp"

namespace FREYA_NAMESPACE
{
    namespace
    {
        bool TextureIdsEqual(const MaterialCreateInfo& a,
                             const MaterialCreateInfo& b)
        {
            return a.albedo == b.albedo && a.normal == b.normal &&
                   a.roughness == b.roughness && a.emissive == b.emissive &&
                   a.metalness == b.metalness;
        }
    } // namespace

    std::uint32_t MaterialPool::CreateFromTextureFiles(
        std::vector<std::string> texturesPath)
    {
        return 0;
    }

    std::uint32_t MaterialPool::Create(const MaterialCreateInfo& createInfo)
    {
        auto material = Material {
            .createInfo = createInfo,
            .id         = static_cast<std::uint32_t>(mMaterials.size()),
        };

        const auto samplerDescriptorSetAllocInfo =
            vk::DescriptorSetAllocateInfo()
                .setSetLayouts(mMaterialsRes->GetSamplerLayout())
                .setDescriptorPool(mMaterialsRes->GetSamplerDescriptorPool());

        material.descriptorSets =
            std::move(mDevice->Get().allocateDescriptorSets(
                samplerDescriptorSetAllocInfo));

        auto factors = PackMaterialFactors(createInfo, material.id);
        material.factorsBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(MaterialFactorsUniform))
                .SetData(&factors)
                .Build();

        writeTextureDescriptors(material, createInfo);
        writeFactorsDescriptor(material);

        mMaterials.insert(material);

        return material.id;
    }

    void MaterialPool::Update(std::uint32_t             id,
                              const MaterialCreateInfo& createInfo)
    {
        auto& material = mMaterials[id];

        if (!TextureIdsEqual(material.createInfo, createInfo))
        {
            writeTextureDescriptors(material, createInfo);
        }

        material.createInfo = createInfo;
        uploadFactors(material);
    }

    const MaterialCreateInfo& MaterialPool::GetCreateInfo(
        std::uint32_t id) const
    {
        return mMaterials[id].createInfo;
    }

    void MaterialPool::writeTextureDescriptors(
        Material& material, const MaterialCreateInfo& createInfo)
    {
        auto& fallbackImageView = mMaterialsRes->GetFallbackImageView();
        auto& fallbackSampler   = mMaterialsRes->GetFallbackSampler();
        auto  fallbackImageInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(fallbackImageView)
                .setSampler(fallbackSampler);

        auto& emissiveFallbackImageView =
            mMaterialsRes->GetEmissiveFallbackImageView();
        auto& emissiveFallbackSampler =
            mMaterialsRes->GetEmissiveFallbackSampler();
        auto emissiveFallbackImageInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(emissiveFallbackImageView)
                .setSampler(emissiveFallbackSampler);

        auto resolveImage = [&](const std::optional<std::uint32_t>& textureId,
                                bool useBlackFallback) {
            if (textureId)
            {
                auto& texture = mTexturePool->GetTexture(*textureId);
                return vk::DescriptorImageInfo()
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSampler(texture.sampler)
                    .setImageView(texture.image->GetImageView());
            }
            return useBlackFallback ? emissiveFallbackImageInfo
                                    : fallbackImageInfo;
        };

        const std::array slots = {
            std::pair { createInfo.albedo, false },
            std::pair { createInfo.normal, false },
            std::pair { createInfo.roughness, false },
            std::pair { createInfo.emissive, true },
            std::pair { createInfo.metalness, true },
        };

        for (std::uint32_t binding = 0; binding < slots.size(); ++binding)
        {
            auto imageInfo =
                resolveImage(slots[binding].first, slots[binding].second);

            auto writer =
                vk::WriteDescriptorSet()
                    .setDstSet(material.descriptorSets[0])
                    .setDstBinding(binding)
                    .setDstArrayElement(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(imageInfo);

            mDevice->Get().updateDescriptorSets(1, &writer, 0, nullptr);
        }
    }

    void MaterialPool::writeFactorsDescriptor(Material& material)
    {
        auto bufferInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(material.factorsBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(MaterialFactorsUniform));

        auto writer =
            vk::WriteDescriptorSet()
                .setDstSet(material.descriptorSets[0])
                .setDstBinding(5)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(bufferInfo);

        mDevice->Get().updateDescriptorSets(1, &writer, 0, nullptr);
    }

    void MaterialPool::uploadFactors(Material& material)
    {
        auto factors = PackMaterialFactors(material.createInfo, material.id);
        material.factorsBuffer->Copy(&factors, sizeof(factors));
    }

    Material& MaterialPool::GetMaterial(uint32_t materialId)
    {
        return mMaterials[materialId];
    }
} // namespace FREYA_NAMESPACE
