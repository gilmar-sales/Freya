#include "MaterialDescriptorResources.hpp"

namespace FREYA_NAMESPACE
{
    MaterialDescriptorResources::MaterialDescriptorResources(
        const skr::Arc<Device>&       device,
        const vk::DescriptorSetLayout bindlessLayout,
        const vk::DescriptorPool      bindlessPool,
        const vk::DescriptorSet       bindlessSet,
        const skr::Arc<Buffer>&       materialsBuffer,
        const vk::Image               fallbackImage,
        const VmaAllocation           fallbackImageMemory,
        const vk::ImageView           fallbackImageView,
        const vk::Sampler             fallbackSampler,
        const vk::Image               emissiveFallbackImage,
        const VmaAllocation           emissiveFallbackMemory,
        const vk::ImageView           emissiveFallbackImageView,
        const vk::Sampler             emissiveFallbackSampler) :
        mDevice(device), mBindlessLayout(bindlessLayout),
        mBindlessPool(bindlessPool), mBindlessSet(bindlessSet),
        mMaterialsBuffer(materialsBuffer), mFallbackImage(fallbackImage),
        mFallbackImageMemory(fallbackImageMemory),
        mFallbackImageView(fallbackImageView),
        mFallbackSampler(fallbackSampler),
        mEmissiveFallbackImage(emissiveFallbackImage),
        mEmissiveFallbackMemory(emissiveFallbackMemory),
        mEmissiveFallbackImageView(emissiveFallbackImageView),
        mEmissiveFallbackSampler(emissiveFallbackSampler)
    {
    }

    MaterialDescriptorResources::~MaterialDescriptorResources()
    {
        mDevice->Get().waitIdle();
        auto& vkDevice = mDevice->Get();

        vkDevice.destroySampler(mFallbackSampler);
        vkDevice.destroyImageView(mFallbackImageView);
        mDevice->GetAllocator()->DestroyImage(
            static_cast<VkImage>(mFallbackImage),
            mFallbackImageMemory);
        mFallbackImageMemory = VK_NULL_HANDLE;

        vkDevice.destroySampler(mEmissiveFallbackSampler);
        vkDevice.destroyImageView(mEmissiveFallbackImageView);
        mDevice->GetAllocator()->DestroyImage(
            static_cast<VkImage>(mEmissiveFallbackImage),
            mEmissiveFallbackMemory);
        mEmissiveFallbackMemory = VK_NULL_HANDLE;

        mMaterialsBuffer.reset();

        vkDevice.destroyDescriptorPool(mBindlessPool);
        vkDevice.destroyDescriptorSetLayout(mBindlessLayout);
    }

    void MaterialDescriptorResources::WriteBindlessTexture(
        const std::uint32_t heapIndex,
        const vk::ImageView imageView,
        const vk::Sampler   sampler)
    {
        if (heapIndex >= kMaxBindlessTextures)
            return;

        const auto imageInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(imageView)
                .setSampler(sampler);

        const auto writer =
            vk::WriteDescriptorSet()
                .setDstSet(mBindlessSet)
                .setDstBinding(0)
                .setDstArrayElement(heapIndex)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(imageInfo);

        mDevice->Get().updateDescriptorSets(1, &writer, 0, nullptr);
    }

    void MaterialDescriptorResources::WriteMaterial(
        const std::uint32_t materialId, const MaterialGPU& gpu)
    {
        if (materialId >= mMaterialCapacity || !mMaterialsBuffer)
            return;

        mMaterialsBuffer->Copy(&gpu,
                               sizeof(MaterialGPU),
                               materialId * sizeof(MaterialGPU));
    }
} // namespace FREYA_NAMESPACE
