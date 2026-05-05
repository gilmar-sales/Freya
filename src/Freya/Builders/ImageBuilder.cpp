#include "Freya/Builders/ImageBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>
#include <cmath>

namespace FREYA_NAMESPACE
{
    Ref<Image> ImageBuilder::Build()
    {
        mLogger->LogTrace("Building 'fra::Image'");

        mLogger->LogTrace("\tSize: {}x{}", mWidth, mHeight);

        if (mFormat == vk::Format::eUndefined)
        {
            chooseFormat();
        }

        // Compute full mip chain for textures
        if (mUsage == ImageUsage::Texture)
        {
            mMipLevels = static_cast<std::uint32_t>(
                             std::floor(std::log2(std::max(mWidth, mHeight)))) +
                         1;
        }

        mLogger->LogTrace("\tMip levels: {}", mMipLevels);

        mLogger->LogTrace("\tFormat: {}", to_string(mFormat));

        auto imageInfo =
            vk::ImageCreateInfo()
                .setExtent(
                    vk::Extent3D().setWidth(mWidth).setHeight(mHeight).setDepth(
                        1))
                .setFormat(mFormat)
                .setTiling(vk::ImageTiling::eOptimal)
                .setMipLevels(mMipLevels)
                .setArrayLayers(1)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setImageType(vk::ImageType::e2D)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setSamples(mSamples);

        switch (mUsage)
        {
            case ImageUsage::Color:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment |
                                   vk::ImageUsageFlagBits::eSampled |
                                   vk::ImageUsageFlagBits::eInputAttachment);
                break;
            case ImageUsage::Depth:
                imageInfo.setUsage(
                    vk::ImageUsageFlagBits::eDepthStencilAttachment |
                    vk::ImageUsageFlagBits::eSampled |
                    vk::ImageUsageFlagBits::eInputAttachment);
                break;
            case ImageUsage::Texture:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eTransferDst |
                                   vk::ImageUsageFlagBits::eTransferSrc |
                                   vk::ImageUsageFlagBits::eSampled);
                break;
            case ImageUsage::Sampling:
                imageInfo.setUsage(
                    vk::ImageUsageFlagBits::eColorAttachment |
                    vk::ImageUsageFlagBits::eTransientAttachment);
                break;
            case ImageUsage::GBufferPosition:
            case ImageUsage::GBufferNormal:
            case ImageUsage::GBufferAlbedo:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment |
                                   vk::ImageUsageFlagBits::eInputAttachment);
                break;
            default:
                break;
        }

        auto image = mDevice->Get().createImage(imageInfo);

        const auto imageRequirements =
            mDevice->Get().getImageMemoryRequirements(image);

        const auto memoryTypeIndex =
            mDevice->GetPhysicalDevice()->QueryCompatibleMemoryType(
                imageRequirements.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eDeviceLocal);

        const auto imageMemoryInfo =
            vk::MemoryAllocateInfo()
                .setAllocationSize(imageRequirements.size)
                .setMemoryTypeIndex(memoryTypeIndex);

        auto imageMemory = mDevice->Get().allocateMemory(imageMemoryInfo);

        mDevice->Get().bindImageMemory(image, imageMemory, 0);

        if (mData != nullptr && mUsage == ImageUsage::Texture)
        {
            const auto commandPool =
                mServiceProvider->GetService<CommandPoolBuilder>()
                    ->SetCount(2)
                    .Build();

            if (mStagingBuffer == nullptr)
                mStagingBuffer =
                    BufferBuilder(mDevice)
                        .SetData(mData)
                        .SetSize(mWidth * mHeight * mChannels)
                        .SetUsage(BufferUsage::Staging)
                        .Build();
            else
                mStagingBuffer->Copy(mData, mWidth * mHeight * mChannels);

            const auto commandBuffer = commandPool->CreateCommandBuffer();

            commandBuffer.begin(vk::CommandBufferBeginInfo().setFlags(
                vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

            // 1. Transition all mip levels: Undefined → TransferDstOptimal
            auto initBarrier =
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
                            .setLevelCount(mMipLevels)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1))
                    .setSrcAccessMask({})
                    .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0,
                nullptr, 0, nullptr, 1, &initBarrier);

            // 2. Copy base level (mip 0) from staging buffer
            const auto imageBufferCopy = {
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
                    .setImageExtent({ mWidth, mHeight, 1 })
            };

            commandBuffer.copyBufferToImage(
                mStagingBuffer->Get(),
                image,
                vk::ImageLayout::eTransferDstOptimal,
                imageBufferCopy);

            // 3. Generate remaining mip levels via vkCmdBlitImage
            for (std::uint32_t i = 1; i < mMipLevels; ++i)
            {
                // Transition mip i-1: TransferDstOptimal → TransferSrcOptimal
                auto srcBarrier =
                    vk::ImageMemoryBarrier()
                        .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                        .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                        .setImage(image)
                        .setSubresourceRange(
                            vk::ImageSubresourceRange()
                                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                .setBaseMipLevel(i - 1)
                                .setLevelCount(1)
                                .setBaseArrayLayer(0)
                                .setLayerCount(1))
                        .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                        .setDstAccessMask(vk::AccessFlagBits::eTransferRead);

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
                    0, nullptr, 0, nullptr, 1, &srcBarrier);

                // Blit from mip i-1 to mip i
                const auto srcW =
                    std::max(1, static_cast<std::int32_t>(mWidth >> (i - 1)));
                const auto srcH =
                    std::max(1, static_cast<std::int32_t>(mHeight >> (i - 1)));
                const auto dstW =
                    std::max(1, static_cast<std::int32_t>(mWidth >> i));
                const auto dstH =
                    std::max(1, static_cast<std::int32_t>(mHeight >> i));

                const auto blitRegion =
                    vk::ImageBlit()
                        .setSrcOffsets({ vk::Offset3D { 0, 0, 0 },
                                         vk::Offset3D { srcW, srcH, 1 } })
                        .setSrcSubresource(
                            vk::ImageSubresourceLayers()
                                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                .setMipLevel(i - 1)
                                .setBaseArrayLayer(0)
                                .setLayerCount(1))
                        .setDstOffsets({ vk::Offset3D { 0, 0, 0 },
                                         vk::Offset3D { dstW, dstH, 1 } })
                        .setDstSubresource(
                            vk::ImageSubresourceLayers()
                                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                .setMipLevel(i)
                                .setBaseArrayLayer(0)
                                .setLayerCount(1));

                commandBuffer.blitImage(
                    image, vk::ImageLayout::eTransferSrcOptimal, image,
                    vk::ImageLayout::eTransferDstOptimal, blitRegion,
                    vk::Filter::eLinear);
            }

            // 4. Transition all mips → ShaderReadOnlyOptimal
            if (mMipLevels == 1)
            {
                // Single level: TransferDstOptimal → ShaderReadOnlyOptimal
                auto finalBarrier =
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
                    &finalBarrier);
            }
            else
            {
                // Mips 0..mMipLevels-2 are in TransferSrcOptimal
                auto srcFinalBarrier =
                    vk::ImageMemoryBarrier()
                        .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
                        .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                        .setImage(image)
                        .setSubresourceRange(
                            vk::ImageSubresourceRange()
                                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                .setBaseMipLevel(0)
                                .setLevelCount(mMipLevels - 1)
                                .setBaseArrayLayer(0)
                                .setLayerCount(1))
                        .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                        .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

                // Mip mMipLevels-1 is in TransferDstOptimal
                auto dstFinalBarrier =
                    vk::ImageMemoryBarrier()
                        .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                        .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                        .setImage(image)
                        .setSubresourceRange(
                            vk::ImageSubresourceRange()
                                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                .setBaseMipLevel(mMipLevels - 1)
                                .setLevelCount(1)
                                .setBaseArrayLayer(0)
                                .setLayerCount(1))
                        .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                        .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

                const vk::ImageMemoryBarrier finalBarriers[] = {
                    srcFinalBarrier, dstFinalBarrier
                };

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::DependencyFlags(), 0, nullptr, 0, nullptr, 2,
                    finalBarriers);
            }

            commandBuffer.end();

            const auto submitInfo =
                vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                    &commandBuffer);

            mDevice->GetTransferQueue().submit(submitInfo);
            mDevice->GetTransferQueue().waitIdle();

            commandPool->FreeCommandBuffer(commandBuffer);
        }

        auto aspect = vk::ImageAspectFlagBits::eNone;

        switch (mUsage)
        {
            case ImageUsage::Color:
            case ImageUsage::Sampling:
            case ImageUsage::Texture:
            case ImageUsage::GBufferPosition:
            case ImageUsage::GBufferNormal:
            case ImageUsage::GBufferAlbedo:
                aspect = vk::ImageAspectFlagBits::eColor;
                break;
            case ImageUsage::Depth:
                aspect = vk::ImageAspectFlagBits::eDepth;
                break;
            default:
                break;
        }

        const auto imageViewInfo =
            vk::ImageViewCreateInfo()
                .setImage(image)
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(mFormat)
                .setComponents(vk::ComponentMapping()
                                   .setR(vk::ComponentSwizzle::eIdentity)
                                   .setG(vk::ComponentSwizzle::eIdentity)
                                   .setB(vk::ComponentSwizzle::eIdentity)
                                   .setA(vk::ComponentSwizzle::eIdentity))
                .setSubresourceRange(vk::ImageSubresourceRange()
                                         .setAspectMask(aspect)
                                         .setBaseMipLevel(0)
                                         .setLevelCount(mMipLevels)
                                         .setBaseArrayLayer(0)
                                         .setLayerCount(1));

        auto imageView = mDevice->Get().createImageView(imageViewInfo);

        return skr::MakeRef<Image>(
            mDevice,
            image,
            imageView,
            imageMemory,
            mFormat,
            mMipLevels);
    }

    vk::Format ImageBuilder::chooseFormat()
    {
        switch (mUsage)
        {
            case ImageUsage::Color:
                mFormat = mSurface->QuerySurfaceFormat().format;
                break;
            case ImageUsage::Depth:
                mFormat = mDevice->GetPhysicalDevice()->GetDepthFormat();
                break;
            case ImageUsage::Texture:
                mFormat = vk::Format::eR8G8B8A8Unorm;
                break;
            case ImageUsage::Sampling:
                mFormat = mSurface->QuerySurfaceFormat().format;
                break;
            case ImageUsage::GBufferPosition:
                mFormat = vk::Format::eR16G16B16A16Sfloat;
                break;
            case ImageUsage::GBufferNormal:
                mFormat = vk::Format::eR16G16B16A16Sfloat;
                break;
            case ImageUsage::GBufferAlbedo:
                mFormat = vk::Format::eR8G8B8A8Srgb;
                break;
            default:
                break;
        }

        return vk::Format();
    }

    void ImageBuilder::transitionLayout(
        const Ref<CommandPool>& commandPool,
        const vk::Image         image,
        const vk::ImageLayout   oldLayout,
        const vk::ImageLayout   newLayout,
        const std::uint32_t     baseMipLevel,
        const std::uint32_t     levelCount) const
    {
        const auto commandBuffer = commandPool->CreateCommandBuffer();

        commandBuffer.begin(vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        auto imageMemoryBarrier =
            vk::ImageMemoryBarrier()
                .setOldLayout(oldLayout)
                .setNewLayout(newLayout)
                .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                .setImage(image)
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(baseMipLevel)
                        .setLevelCount(levelCount)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1));

        vk::PipelineStageFlags srcStage, dstStage;

        if (oldLayout == vk::ImageLayout::eUndefined &&
            newLayout == vk::ImageLayout::eTransferDstOptimal)
        {
            imageMemoryBarrier.setSrcAccessMask({}).setDstAccessMask(
                vk::AccessFlagBits::eTransferWrite);

            srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
            dstStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
                 newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            imageMemoryBarrier
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

            srcStage = vk::PipelineStageFlagBits::eTransfer;
            dstStage = vk::PipelineStageFlagBits::eFragmentShader;
        }

        commandBuffer.pipelineBarrier(
            srcStage,
            dstStage,
            vk::DependencyFlags(),
            0,
            nullptr,
            0,
            nullptr,
            1,
            &imageMemoryBarrier);

        commandBuffer.end();

        const auto submitInfo =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                &commandBuffer);

        mDevice->GetTransferQueue().submit(submitInfo);
        mDevice->GetTransferQueue().waitIdle();

        commandPool->FreeCommandBuffer(commandBuffer);
    }

}; // namespace FREYA_NAMESPACE