#include "Freya/Builders/ImageBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>
#include <cmath>

namespace FREYA_NAMESPACE
{
    skr::Arc<Image> ImageBuilder::Build()
    {
        mLogger->LogTrace("Building 'fra::Image'");

        mLogger->LogTrace("\tSize: {}x{}", mWidth, mHeight);

        if (mFormat == vk::Format::eUndefined)
        {
            chooseFormat();
        }

        // Compute full mip chain for textures unless overridden
        if (mUsage == ImageUsage::Texture && !mMipLevelsOverride)
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
                                   vk::ImageUsageFlagBits::eInputAttachment |
                                   vk::ImageUsageFlagBits::eTransferSrc |
                                   vk::ImageUsageFlagBits::eTransferDst);
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
            case ImageUsage::GBufferAlbedo:
            case ImageUsage::GBufferNormal:
            case ImageUsage::GBufferPbr:
            case ImageUsage::GBufferVelocity:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment |
                                   vk::ImageUsageFlagBits::eInputAttachment |
                                   vk::ImageUsageFlagBits::eSampled);
                break;
            case ImageUsage::GBufferSceneColor:
                // Geometry emissive + lighting accumulation; sampled by
                // bloom/TAA; Transfer* lets PostProcess blit results
                // back onto the HDR target.
                imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment |
                                   vk::ImageUsageFlagBits::eSampled |
                                   vk::ImageUsageFlagBits::eTransferSrc |
                                   vk::ImageUsageFlagBits::eTransferDst);
                break;
            case ImageUsage::TaaHistory:
            case ImageUsage::TaaDepthHistory:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eStorage |
                                   vk::ImageUsageFlagBits::eSampled |
                                   vk::ImageUsageFlagBits::eTransferDst);
                break;
            case ImageUsage::HiZDepth:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eStorage |
                                   vk::ImageUsageFlagBits::eSampled |
                                   vk::ImageUsageFlagBits::eTransferSrc |
                                   vk::ImageUsageFlagBits::eTransferDst);
                break;
            case ImageUsage::Ssao:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eStorage |
                                   vk::ImageUsageFlagBits::eSampled);
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

            const bool uploadCustomMips =
                mUploadCustomMipChain && mMipLevels > 1;

            std::uint64_t uploadBytes =
                static_cast<std::uint64_t>(mWidth) * mHeight * mChannels;
            if (uploadCustomMips)
            {
                uploadBytes = 0;
                for (std::uint32_t mip = 0; mip < mMipLevels; ++mip)
                {
                    const auto mipW = std::max(1u, mWidth >> mip);
                    const auto mipH = std::max(1u, mHeight >> mip);
                    uploadBytes +=
                        static_cast<std::uint64_t>(mipW) * mipH * mChannels;
                }
            }

            if (mStagingBuffer == nullptr)
                mStagingBuffer =
                    BufferBuilder(mDevice)
                        .SetData(mData)
                        .SetSize(uploadBytes)
                        .SetUsage(BufferUsage::Staging)
                        .Build();
            else
                mStagingBuffer->Copy(mData, uploadBytes);

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

            if (uploadCustomMips)
            {
                // 2. Copy each packed mip from staging (no blit).
                std::uint64_t bufferOffset = 0;
                for (std::uint32_t mip = 0; mip < mMipLevels; ++mip)
                {
                    const auto mipW = std::max(1u, mWidth >> mip);
                    const auto mipH = std::max(1u, mHeight >> mip);
                    const auto region =
                        vk::BufferImageCopy()
                            .setBufferOffset(bufferOffset)
                            .setBufferRowLength(0)
                            .setBufferImageHeight(0)
                            .setImageSubresource(
                                vk::ImageSubresourceLayers()
                                    .setAspectMask(
                                        vk::ImageAspectFlagBits::eColor)
                                    .setMipLevel(mip)
                                    .setBaseArrayLayer(0)
                                    .setLayerCount(1))
                            .setImageOffset({ 0, 0, 0 })
                            .setImageExtent({ mipW, mipH, 1 });

                    commandBuffer.copyBufferToImage(
                        mStagingBuffer->Get(),
                        image,
                        vk::ImageLayout::eTransferDstOptimal,
                        region);

                    bufferOffset +=
                        static_cast<std::uint64_t>(mipW) * mipH * mChannels;
                }

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
                                .setLevelCount(mMipLevels)
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
                    // Transition mip i-1: TransferDst → TransferSrc
                    auto srcBarrier =
                        vk::ImageMemoryBarrier()
                            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                            .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                            .setImage(image)
                            .setSubresourceRange(
                                vk::ImageSubresourceRange()
                                    .setAspectMask(
                                        vk::ImageAspectFlagBits::eColor)
                                    .setBaseMipLevel(i - 1)
                                    .setLevelCount(1)
                                    .setBaseArrayLayer(0)
                                    .setLayerCount(1))
                            .setSrcAccessMask(
                                vk::AccessFlagBits::eTransferWrite)
                            .setDstAccessMask(
                                vk::AccessFlagBits::eTransferRead);

                    commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1,
                        &srcBarrier);

                    const auto srcW = std::max(
                        1, static_cast<std::int32_t>(mWidth >> (i - 1)));
                    const auto srcH = std::max(
                        1, static_cast<std::int32_t>(mHeight >> (i - 1)));
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
                                    .setAspectMask(
                                        vk::ImageAspectFlagBits::eColor)
                                    .setMipLevel(i - 1)
                                    .setBaseArrayLayer(0)
                                    .setLayerCount(1))
                            .setDstOffsets({ vk::Offset3D { 0, 0, 0 },
                                             vk::Offset3D { dstW, dstH, 1 } })
                            .setDstSubresource(
                                vk::ImageSubresourceLayers()
                                    .setAspectMask(
                                        vk::ImageAspectFlagBits::eColor)
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
                    auto finalBarrier =
                        vk::ImageMemoryBarrier()
                            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                            .setNewLayout(
                                vk::ImageLayout::eShaderReadOnlyOptimal)
                            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                            .setImage(image)
                            .setSubresourceRange(
                                vk::ImageSubresourceRange()
                                    .setAspectMask(
                                        vk::ImageAspectFlagBits::eColor)
                                    .setBaseMipLevel(0)
                                    .setLevelCount(1)
                                    .setBaseArrayLayer(0)
                                    .setLayerCount(1))
                            .setSrcAccessMask(
                                vk::AccessFlagBits::eTransferWrite)
                            .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

                    commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1,
                        &finalBarrier);
                }
                else
                {
                    auto srcFinalBarrier =
                        vk::ImageMemoryBarrier()
                            .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
                            .setNewLayout(
                                vk::ImageLayout::eShaderReadOnlyOptimal)
                            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                            .setImage(image)
                            .setSubresourceRange(
                                vk::ImageSubresourceRange()
                                    .setAspectMask(
                                        vk::ImageAspectFlagBits::eColor)
                                    .setBaseMipLevel(0)
                                    .setLevelCount(mMipLevels - 1)
                                    .setBaseArrayLayer(0)
                                    .setLayerCount(1))
                            .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                            .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

                    auto dstFinalBarrier =
                        vk::ImageMemoryBarrier()
                            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                            .setNewLayout(
                                vk::ImageLayout::eShaderReadOnlyOptimal)
                            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                            .setImage(image)
                            .setSubresourceRange(
                                vk::ImageSubresourceRange()
                                    .setAspectMask(
                                        vk::ImageAspectFlagBits::eColor)
                                    .setBaseMipLevel(mMipLevels - 1)
                                    .setLevelCount(1)
                                    .setBaseArrayLayer(0)
                                    .setLayerCount(1))
                            .setSrcAccessMask(
                                vk::AccessFlagBits::eTransferWrite)
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
            case ImageUsage::GBufferAlbedo:
            case ImageUsage::GBufferNormal:
            case ImageUsage::GBufferPbr:
            case ImageUsage::GBufferSceneColor:
            case ImageUsage::GBufferVelocity:
            case ImageUsage::TaaHistory:
            case ImageUsage::TaaDepthHistory:
            case ImageUsage::HiZDepth:
            case ImageUsage::Ssao:
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

        return skr::MakeArc<Image>(
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
                if (mChannels == 1)
                    mFormat = vk::Format::eR8Unorm;
                else
                    mFormat = vk::Format::eR8G8B8A8Unorm;
                break;
            case ImageUsage::Sampling:
                mFormat = mSurface->QuerySurfaceFormat().format;
                break;
            case ImageUsage::GBufferAlbedo:
                // UNORM: albedo in gamma space; mat ID in A (not sRGB)
                mFormat = vk::Format::eR8G8B8A8Unorm;
                break;
            case ImageUsage::GBufferNormal:
                mFormat = vk::Format::eA2B10G10R10UnormPack32;
                break;
            case ImageUsage::GBufferPbr:
                // R roughness, G metallic, B AO or coat roughness, A clearcoat
                mFormat = vk::Format::eR8G8B8A8Unorm;
                break;
            case ImageUsage::GBufferSceneColor:
                mFormat = vk::Format::eR16G16B16A16Sfloat;
                break;
            case ImageUsage::GBufferVelocity:
                mFormat = vk::Format::eR16G16Sfloat;
                break;
            case ImageUsage::TaaHistory:
                mFormat = vk::Format::eR16G16B16A16Sfloat;
                break;
            case ImageUsage::TaaDepthHistory:
                mFormat = vk::Format::eR16Sfloat;
                break;
            case ImageUsage::HiZDepth:
                mFormat = vk::Format::eR32Sfloat;
                break;
            case ImageUsage::Ssao:
                mFormat = vk::Format::eR8Unorm;
                break;
            default:
                break;
        }

        return vk::Format();
    }

    void ImageBuilder::transitionLayout(
        const skr::Arc<CommandPool>& commandPool,
        const vk::Image              image,
        const vk::ImageLayout        oldLayout,
        const vk::ImageLayout        newLayout,
        const std::uint32_t          baseMipLevel,
        const std::uint32_t          levelCount) const
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