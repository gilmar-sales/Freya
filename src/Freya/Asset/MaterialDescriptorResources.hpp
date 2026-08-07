#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * Shared material set-1 layout, descriptor pool, and 1x1 fallback textures.
     *
     * Owned independently of any graphics pass so MaterialPool / TexturePool
     * and DeferredCompressedPass can share the same sampler layout handle.
     */
    class MaterialDescriptorResources
    {
      public:
        MaterialDescriptorResources(
            const skr::Arc<Device>& device,
            vk::DescriptorSetLayout samplerLayout,
            vk::DescriptorPool      samplerDescriptorPool,
            vk::DescriptorSet       fallbackSamplerSet,
            const skr::Arc<Buffer>& fallbackFactorsBuffer,
            vk::Image               fallbackImage,
            vk::DeviceMemory        fallbackImageMemory,
            vk::ImageView           fallbackImageView,
            vk::Sampler             fallbackSampler,
            vk::Image               emissiveFallbackImage,
            vk::DeviceMemory        emissiveFallbackMemory,
            vk::ImageView           emissiveFallbackImageView,
            vk::Sampler             emissiveFallbackSampler);

        ~MaterialDescriptorResources();

        vk::DescriptorSetLayout& GetSamplerLayout() { return mSamplerLayout; }

        vk::DescriptorPool& GetSamplerDescriptorPool()
        {
            return mSamplerDescriptorPool;
        }

        vk::DescriptorSet& GetFallbackSamplerSet()
        {
            return mFallbackSamplerSet;
        }

        vk::ImageView& GetFallbackImageView() { return mFallbackImageView; }

        vk::Sampler& GetFallbackSampler() { return mFallbackSampler; }

        vk::ImageView& GetEmissiveFallbackImageView()
        {
            return mEmissiveFallbackImageView;
        }

        vk::Sampler& GetEmissiveFallbackSampler()
        {
            return mEmissiveFallbackSampler;
        }

      private:
        skr::Arc<Device> mDevice;

        vk::DescriptorSetLayout mSamplerLayout;
        vk::DescriptorPool      mSamplerDescriptorPool;
        vk::DescriptorSet       mFallbackSamplerSet;
        skr::Arc<Buffer>        mFallbackFactorsBuffer;

        vk::Image        mFallbackImage;
        vk::DeviceMemory mFallbackImageMemory;
        vk::ImageView    mFallbackImageView;
        vk::Sampler      mFallbackSampler;

        vk::Image        mEmissiveFallbackImage;
        vk::DeviceMemory mEmissiveFallbackMemory;
        vk::ImageView    mEmissiveFallbackImageView;
        vk::Sampler      mEmissiveFallbackSampler;
    };
} // namespace FREYA_NAMESPACE
