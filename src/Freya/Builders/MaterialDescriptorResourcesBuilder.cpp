#include "MaterialDescriptorResourcesBuilder.hpp"

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/UniformBuffer.hpp"

#include <array>
#include <tuple>
#include <vector>

namespace FREYA_NAMESPACE
{
    skr::Arc<MaterialDescriptorResources> MaterialDescriptorResourcesBuilder::
        Build()
    {
        if (mLogger)
            mLogger->LogTrace("Building 'fra::MaterialDescriptorResources':");

        constexpr auto           maxMaterialSets = MAX_MATERIAL_SETS;
        constexpr std::uint32_t  whitePixel      = 0xFFFFFFFF;
        constexpr std::uint32_t  blackPixel      = 0xFF000000;
        constexpr vk::DeviceSize pixelSize       = sizeof(whitePixel);
        constexpr std::uint32_t  texWidth        = 1;
        constexpr std::uint32_t  texHeight       = 1;

        const auto createFallbackTexture = [&](std::uint32_t pixel) {
            const auto staging =
                BufferBuilder(mDevice)
                    .SetData(&pixel)
                    .SetSize(pixelSize)
                    .SetUsage(BufferUsage::Staging)
                    .Build();

            const auto imageInfo =
                vk::ImageCreateInfo()
                    .setImageType(vk::ImageType::e2D)
                    .setFormat(vk::Format::eR8G8B8A8Unorm)
                    .setExtent({ texWidth, texHeight, 1 })
                    .setMipLevels(1)
                    .setArrayLayers(1)
                    .setSamples(vk::SampleCountFlagBits::e1)
                    .setTiling(vk::ImageTiling::eOptimal)
                    .setUsage(vk::ImageUsageFlagBits::eTransferDst |
                              vk::ImageUsageFlagBits::eSampled)
                    .setSharingMode(vk::SharingMode::eExclusive)
                    .setInitialLayout(vk::ImageLayout::eUndefined);
            const auto image = mDevice->Get().createImage(imageInfo);

            const auto memoryRequirements =
                mDevice->Get().getImageMemoryRequirements(image);
            const auto memoryType = mPhysicalDevice->QueryCompatibleMemoryType(
                memoryRequirements.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eDeviceLocal);
            const auto allocationInfo =
                vk::MemoryAllocateInfo()
                    .setAllocationSize(memoryRequirements.size)
                    .setMemoryTypeIndex(memoryType);
            const auto imageMemory =
                mDevice->Get().allocateMemory(allocationInfo);
            mDevice->Get().bindImageMemory(image, imageMemory, 0);

            const auto graphicsFamily =
                mDevice->GetQueueFamilyIndices().graphicsFamily.value();
            const auto commandPool = mDevice->Get().createCommandPool(
                vk::CommandPoolCreateInfo()
                    .setQueueFamilyIndex(graphicsFamily)
                    .setFlags(vk::CommandPoolCreateFlagBits::eTransient));
            const auto commandBuffers = mDevice->Get().allocateCommandBuffers(
                vk::CommandBufferAllocateInfo()
                    .setCommandPool(commandPool)
                    .setLevel(vk::CommandBufferLevel::ePrimary)
                    .setCommandBufferCount(1));
            const auto commandBuffer = commandBuffers[0];

            commandBuffer.begin(vk::CommandBufferBeginInfo().setFlags(
                vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

            const auto transferBarrier =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eUndefined)
                    .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                    .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                    .setImage(image)
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1))
                    .setSrcAccessMask({})
                    .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0,
                nullptr, 0, nullptr, 1, &transferBarrier);

            const auto copyRegion =
                vk::BufferImageCopy()
                    .setBufferOffset(0)
                    .setBufferRowLength(0)
                    .setBufferImageHeight(0)
                    .setImageSubresource(
                        vk::ImageSubresourceLayers()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setMipLevel(0)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1))
                    .setImageOffset({ 0, 0, 0 })
                    .setImageExtent({ texWidth, texHeight, 1 });
            commandBuffer.copyBufferToImage(
                staging->Get(), image, vk::ImageLayout::eTransferDstOptimal,
                copyRegion);

            const auto shaderReadBarrier =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                    .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                    .setImage(image)
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1))
                    .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                    .setDstAccessMask(vk::AccessFlagBits::eShaderRead);
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1,
                &shaderReadBarrier);

            commandBuffer.end();
            const auto submitInfo =
                vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                    &commandBuffer);
            mDevice->GetGraphicsQueue().submit(submitInfo);
            mDevice->GetGraphicsQueue().waitIdle();
            mDevice->Get().destroyCommandPool(commandPool);

            const auto imageView = mDevice->Get().createImageView(
                vk::ImageViewCreateInfo()
                    .setImage(image)
                    .setViewType(vk::ImageViewType::e2D)
                    .setFormat(vk::Format::eR8G8B8A8Unorm)
                    .setComponents(vk::ComponentMapping()
                                       .setR(vk::ComponentSwizzle::eIdentity)
                                       .setG(vk::ComponentSwizzle::eIdentity)
                                       .setB(vk::ComponentSwizzle::eIdentity)
                                       .setA(vk::ComponentSwizzle::eIdentity))
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1)));

            const auto sampler = mDevice->Get().createSampler(
                vk::SamplerCreateInfo()
                    .setMagFilter(vk::Filter::eLinear)
                    .setMinFilter(vk::Filter::eLinear)
                    .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                    .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                    .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                    .setAnisotropyEnable(false)
                    .setMaxAnisotropy(1.0f)
                    .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
                    .setUnnormalizedCoordinates(false)
                    .setCompareEnable(false)
                    .setCompareOp(vk::CompareOp::eAlways)
                    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                    .setMipLodBias(0.0f)
                    .setMinLod(0.0f)
                    .setMaxLod(0.0f));

            return std::tuple { image, imageMemory, imageView, sampler };
        };

        const auto [fallbackImage, fallbackImageMemory, fallbackImageView,
                    fallbackSampler] = createFallbackTexture(whitePixel);
        const auto [emissiveFallbackImage, emissiveFallbackMemory,
                    emissiveFallbackImageView, emissiveFallbackSampler] =
            createFallbackTexture(blackPixel);

        const auto whiteImageInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(fallbackImageView)
                .setSampler(fallbackSampler);
        const auto blackImageInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(emissiveFallbackImageView)
                .setSampler(emissiveFallbackSampler);

        // Bindless set-1: textures[] + MaterialGPU SSBO
        const auto bindlessFlags = std::array {
            vk::DescriptorBindingFlags {
                vk::DescriptorBindingFlagBits::ePartiallyBound |
                vk::DescriptorBindingFlagBits::eUpdateAfterBind },
            vk::DescriptorBindingFlags {},
        };
        const auto bindingFlagsInfo =
            vk::DescriptorSetLayoutBindingFlagsCreateInfo().setBindingFlags(
                bindlessFlags);

        const auto textureBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(kMaxBindlessTextures)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment);
        const auto materialBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment);

        const auto bindlessBindings =
            std::array { textureBinding, materialBinding };
        const auto bindlessLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo()
                .setPNext(&bindingFlagsInfo)
                .setFlags(
                    vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool)
                .setBindings(bindlessBindings));

        const auto bindlessPoolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(kMaxBindlessTextures),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1),
        };
        const auto bindlessPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
                .setPoolSizes(bindlessPoolSizes)
                .setMaxSets(1));

        const auto bindlessSets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(bindlessPool)
                .setSetLayouts(bindlessLayout));
        const auto bindlessSet = bindlessSets[0];

        std::vector<MaterialGPU> materialTable(maxMaterialSets);
        const auto               materialsBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Storage)
                .SetSize(sizeof(MaterialGPU) * maxMaterialSets)
                .SetData(materialTable.data())
                .Build();

        const auto materialsBufferInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(materialsBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(MaterialGPU) * maxMaterialSets);

        // Fill entire bindless heap with white (slot 1 = black override below).
        std::vector<vk::DescriptorImageInfo> heapInfos(kMaxBindlessTextures,
                                                       whiteImageInfo);
        heapInfos[kBindlessBlackTexture] = blackImageInfo;

        auto texturesWrite =
            vk::WriteDescriptorSet()
                .setDstSet(bindlessSet)
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(kMaxBindlessTextures)
                .setPImageInfo(heapInfos.data());
        auto materialsWrite =
            vk::WriteDescriptorSet()
                .setDstSet(bindlessSet)
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(materialsBufferInfo);
        const auto bindlessWrites =
            std::array { texturesWrite, materialsWrite };
        mDevice->Get().updateDescriptorSets(bindlessWrites, nullptr);

        return skr::MakeArc<MaterialDescriptorResources>(
            mDevice, bindlessLayout, bindlessPool, bindlessSet, materialsBuffer,
            fallbackImage, fallbackImageMemory, fallbackImageView,
            fallbackSampler, emissiveFallbackImage, emissiveFallbackMemory,
            emissiveFallbackImageView, emissiveFallbackSampler);
    }
} // namespace FREYA_NAMESPACE
