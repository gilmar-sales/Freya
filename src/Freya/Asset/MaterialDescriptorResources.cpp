#include "MaterialDescriptorResources.hpp"

namespace FREYA_NAMESPACE
{
    MaterialDescriptorResources::MaterialDescriptorResources(
        const skr::Arc<Device>&       device,
        const vk::DescriptorSetLayout samplerLayout,
        const vk::DescriptorPool      samplerDescriptorPool,
        const vk::DescriptorSet       fallbackSamplerSet,
        const skr::Arc<Buffer>&       fallbackFactorsBuffer,
        const vk::Image               fallbackImage,
        const vk::DeviceMemory        fallbackImageMemory,
        const vk::ImageView           fallbackImageView,
        const vk::Sampler             fallbackSampler,
        const vk::Image               emissiveFallbackImage,
        const vk::DeviceMemory        emissiveFallbackMemory,
        const vk::ImageView           emissiveFallbackImageView,
        const vk::Sampler             emissiveFallbackSampler) :
        mDevice(device), mSamplerLayout(samplerLayout),
        mSamplerDescriptorPool(samplerDescriptorPool),
        mFallbackSamplerSet(fallbackSamplerSet),
        mFallbackFactorsBuffer(fallbackFactorsBuffer),
        mFallbackImage(fallbackImage),
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
        auto& vkDevice = mDevice->Get();

        vkDevice.destroySampler(mFallbackSampler);
        vkDevice.destroyImageView(mFallbackImageView);
        vkDevice.destroyImage(mFallbackImage);
        vkDevice.freeMemory(mFallbackImageMemory);

        vkDevice.destroySampler(mEmissiveFallbackSampler);
        vkDevice.destroyImageView(mEmissiveFallbackImageView);
        vkDevice.destroyImage(mEmissiveFallbackImage);
        vkDevice.freeMemory(mEmissiveFallbackMemory);

        mFallbackFactorsBuffer.reset();

        vkDevice.destroyDescriptorPool(mSamplerDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mSamplerLayout);
    }
} // namespace FREYA_NAMESPACE
