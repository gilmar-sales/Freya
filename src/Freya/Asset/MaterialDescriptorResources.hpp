#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/UniformBuffer.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * Bindless material heap: textures[] + MaterialGPU SSBO (set 1).
     */
    class MaterialDescriptorResources
    {
      public:
        MaterialDescriptorResources(
            const skr::Arc<Device>& device,
            vk::DescriptorSetLayout bindlessLayout,
            vk::DescriptorPool      bindlessPool,
            vk::DescriptorSet       bindlessSet,
            const skr::Arc<Buffer>& materialsBuffer,
            vk::Image               fallbackImage,
            vk::DeviceMemory        fallbackImageMemory,
            vk::ImageView           fallbackImageView,
            vk::Sampler             fallbackSampler,
            vk::Image               emissiveFallbackImage,
            vk::DeviceMemory        emissiveFallbackMemory,
            vk::ImageView           emissiveFallbackImageView,
            vk::Sampler             emissiveFallbackSampler);

        ~MaterialDescriptorResources();

        vk::DescriptorSetLayout& GetBindlessLayout() { return mBindlessLayout; }

        vk::DescriptorSet& GetBindlessSet() { return mBindlessSet; }

        skr::Arc<Buffer>& GetMaterialsBuffer() { return mMaterialsBuffer; }

        /**
         * @brief Map a TexturePool id onto the bindless heap (id + 2).
         */
        static std::uint32_t TextureHeapIndex(std::uint32_t textureId)
        {
            return textureId + 2;
        }

        void WriteBindlessTexture(std::uint32_t heapIndex,
                                  vk::ImageView imageView,
                                  vk::Sampler   sampler);

        void WriteMaterial(std::uint32_t materialId, const MaterialGPU& gpu);

        [[nodiscard]] std::uint32_t GetMaterialCapacity() const
        {
            return mMaterialCapacity;
        }

      private:
        skr::Arc<Device> mDevice;

        vk::DescriptorSetLayout mBindlessLayout;
        vk::DescriptorPool      mBindlessPool;
        vk::DescriptorSet       mBindlessSet;
        skr::Arc<Buffer>        mMaterialsBuffer;
        std::uint32_t           mMaterialCapacity = MAX_MATERIAL_SETS;

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
