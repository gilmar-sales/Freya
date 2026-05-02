#include "MaterialPool.hpp"

namespace FREYA_NAMESPACE
{
    std::uint32_t MaterialPool::CreateFromTextureFiles(
        std::vector<std::string> texturesPath)
    {
        return 0;
    }

    void MaterialPool::ensureBindlessDescriptorSet()
    {
        if (mBindlessSamplerSet)
            return;

        // Lazily allocate the single global bindless sampler descriptor set.
        // The pool and layout were created by RenderPassBuilder.
        auto layout = mRenderPass->GetSamplerLayout();
        auto pool   = mRenderPass->GetSamplerDescriptorPool();

        const auto allocInfo =
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(pool)
                .setSetLayouts(layout);

        auto sets = mDevice->Get().allocateDescriptorSets(allocInfo);
        mBindlessSamplerSet = sets[0];

        mLogger->LogInformation(
            "Allocated global bindless sampler descriptor set");
    }

    std::uint32_t MaterialPool::Create(std::vector<std::uint32_t> textures)
    {
        // Ensure the bindless descriptor set is allocated on first material
        // creation (deferred from constructor to avoid pool timing issues).
        ensureBindlessDescriptorSet();

        const auto materialId =
            static_cast<std::uint32_t>(mMaterials.size());

        auto material = Material {
            .descriptorSets = {}, // Bindless: no per-material descriptor sets
            .id             = materialId,
        };

        // Update the global bindless sampler set at this material's index.
        // For each texture binding (albedo=0, normal=1, roughness=2), write
        // the texture's image info into the corresponding array element.
        for (size_t binding = 0; binding < textures.size(); binding++)
        {
            auto& texture = mTexturePool->GetTexture(textures[binding]);

            auto descriptorImageInfo =
                vk::DescriptorImageInfo()
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSampler(texture.sampler)
                    .setImageView(texture.image->GetImageView());

            auto descriptorWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(mBindlessSamplerSet)
                    .setDstBinding(static_cast<std::uint32_t>(binding))
                    .setDstArrayElement(materialId)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(descriptorImageInfo);

            mDevice->Get()
                .updateDescriptorSets(1, &descriptorWriter, 0, nullptr);
        }

        mMaterials.insert(material);

        mLogger->LogTrace(
            "Registered bindless material {} at array index {}",
            material.id, materialId);

        return material.id;
    }

    void MaterialPool::Bind(std::uint32_t materialId)
    {
        // Bindless mode: bind the global sampler set at set 1.
        // The materialId is used by the shader via nonuniformEXT() indexing
        // into the texture arrays from the draw metadata buffer.
        static_cast<void>(materialId);

        if (!mBindlessSamplerSet)
            return;

        mCommandPool->GetCommandBuffer().bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mRenderPass->GetPipelineLayout(),
            1, // set 1 = sampler arrays
            1, &mBindlessSamplerSet,
            0, nullptr);
    }
} // namespace FREYA_NAMESPACE
