#include "Freya/Builders/ImageBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

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

        auto format = to_string(mFormat);
        mLogger->LogTrace("\tFormat: {}", format);

        auto imageInfo =
            vk::ImageCreateInfo()
                .setExtent(
                    vk::Extent3D().setWidth(mWidth).setHeight(mHeight).setDepth(
                        1))
                .setFormat(mFormat)
                .setTiling(vk::ImageTiling::eOptimal)
                .setMipLevels(1)
                .setArrayLayers(1)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setImageType(vk::ImageType::e2D)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setSamples(mSamples);

        switch (mUsage)
        {
            case ImageUsage::Color:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment);
                break;
            case ImageUsage::Depth:
                imageInfo.setUsage(
                    vk::ImageUsageFlagBits::eDepthStencilAttachment);
                break;
            case ImageUsage::Texture:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eTransferDst |
                                   vk::ImageUsageFlagBits::eSampled);
                break;
            case ImageUsage::Sampling:
                imageInfo.setUsage(
                    vk::ImageUsageFlagBits::eColorAttachment |
                    vk::ImageUsageFlagBits::eTransientAttachment);
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

        auto priorityInfo =
            vk::MemoryPriorityAllocateInfoEXT().setPriority(0.5f);

        switch (mUsage)
        {
            case ImageUsage::Color:
            case ImageUsage::Depth:
            case ImageUsage::Sampling:
                priorityInfo.setPriority(1.0f);
            case ImageUsage::Texture:
                priorityInfo.setPriority(0.8f);
            default:
                break;
        }

        const auto imageMemoryInfo =
            vk::MemoryAllocateInfo()
                .setAllocationSize(imageRequirements.size)
                .setMemoryTypeIndex(memoryTypeIndex)
                .setPNext(&priorityInfo);

        auto imageMemory = mDevice->Get().allocateMemory(imageMemoryInfo);

        mDevice->Get().bindImageMemory(image, imageMemory, 0);

        if (mData != nullptr && mUsage == ImageUsage::Texture)
        {
            const auto commandPool =
                CommandPoolBuilder().SetDevice(mDevice).SetCount(2).Build();

            if (mStagingBuffer == nullptr)
                mStagingBuffer =
                    BufferBuilder(mDevice)
                        .SetData(mData)
                        .SetSize(mWidth * mHeight * mChannels)
                        .SetUsage(BufferUsage::Staging)
                        .Build();
            else
                mStagingBuffer->Copy(mData, mWidth * mHeight * mChannels);

            transitionLayout(commandPool,
                             image,
                             vk::ImageLayout::eUndefined,
                             vk::ImageLayout::eTransferDstOptimal);

            const auto commandBuffer = commandPool->CreateCommandBuffer();

            commandBuffer.begin(vk::CommandBufferBeginInfo().setFlags(
                vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

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

            commandBuffer.end();

            const auto submitInfo =
                vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                    &commandBuffer);

            mDevice->GetTransferQueue().submit(submitInfo);
            mDevice->GetTransferQueue().waitIdle();

            commandPool->FreeCommandBuffer(commandBuffer);

            transitionLayout(commandPool,
                             image,
                             vk::ImageLayout::eTransferDstOptimal,
                             vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        auto aspect = vk::ImageAspectFlagBits::eNone;

        switch (mUsage)
        {
            case ImageUsage::Color:
            case ImageUsage::Sampling:
            case ImageUsage::Texture:
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
                                         .setLevelCount(1)
                                         .setBaseArrayLayer(0)
                                         .setLayerCount(1));

        auto imageView = mDevice->Get().createImageView(imageViewInfo);

        return MakeRef<Image>(mDevice, image, imageView, imageMemory, mFormat);
    }

    vk::Format ImageBuilder::chooseFormat()
    {
        switch (mUsage)
        {
            case ImageUsage::Color:
                mFormat = mDevice->GetSurface()->QuerySurfaceFormat().format;
                break;
            case ImageUsage::Depth:
                mFormat = mDevice->GetPhysicalDevice()->GetDepthFormat();
                break;
            case ImageUsage::Texture:
                mFormat = vk::Format::eR8G8B8A8Unorm;
                break;
            case ImageUsage::Sampling:
                mFormat = mDevice->GetSurface()->QuerySurfaceFormat().format;
                break;
            default:
                break;
        }

        return vk::Format();
    }

    void ImageBuilder::transitionLayout(const Ref<CommandPool>& commandPool,
                                        const vk::Image         image,
                                        const vk::ImageLayout   oldLayout,
                                        const vk::ImageLayout   newLayout) const
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
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
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