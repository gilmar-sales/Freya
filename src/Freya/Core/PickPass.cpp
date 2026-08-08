#include "Freya/Core/PickPass.hpp"

#include <algorithm>
#include <array>

namespace FREYA_NAMESPACE
{
    PickPass::PickPass(
        const skr::Arc<Device>&         device,
        const skr::Arc<PhysicalDevice>& physicalDevice,
        const skr::Arc<FreyaOptions>&   freyaOptions,
        const vk::RenderPass            renderPass,
        const vk::PipelineLayout        pipelineLayout,
        const vk::Pipeline              pipeline,
        const vk::DescriptorSetLayout   descriptorSetLayout,
        const vk::DescriptorPool        descriptorPool,
        const vk::DescriptorSet         descriptorSet,
        const skr::Arc<Buffer>&         uniformBuffer,
        const skr::Arc<Buffer>&         stagingBuffer,
        const skr::Arc<Image>&          colorImage,
        const skr::Arc<Image>&          depthImage,
        const vk::Framebuffer           framebuffer,
        const vk::Extent2D              extent) :
        mDevice(device), mPhysicalDevice(physicalDevice),
        mFreyaOptions(freyaOptions), mRenderPass(renderPass),
        mPipelineLayout(pipelineLayout), mPipeline(pipeline),
        mDescriptorSetLayout(descriptorSetLayout),
        mDescriptorPool(descriptorPool), mDescriptorSet(descriptorSet),
        mUniformBuffer(uniformBuffer), mStagingBuffer(stagingBuffer),
        mColorImage(colorImage), mDepthImage(depthImage),
        mFramebuffer(framebuffer), mExtent(extent)
    {
    }

    PickPass::~PickPass()
    {
        mDevice->Get().waitIdle();
        destroyFramebufferResources();

        if (mPipeline)
        {
            mDevice->Get().destroyPipeline(mPipeline);
            mPipeline = VK_NULL_HANDLE;
        }
        if (mPipelineLayout)
        {
            mDevice->Get().destroyPipelineLayout(mPipelineLayout);
            mPipelineLayout = VK_NULL_HANDLE;
        }
        if (mRenderPass)
        {
            mDevice->Get().destroyRenderPass(mRenderPass);
            mRenderPass = VK_NULL_HANDLE;
        }
        if (mDescriptorPool)
        {
            mDevice->Get().destroyDescriptorPool(mDescriptorPool);
            mDescriptorPool = VK_NULL_HANDLE;
        }
        if (mDescriptorSetLayout)
        {
            mDevice->Get().destroyDescriptorSetLayout(mDescriptorSetLayout);
            mDescriptorSetLayout = VK_NULL_HANDLE;
        }
    }

    void PickPass::destroyFramebufferResources()
    {
        if (mFramebuffer)
        {
            mDevice->Get().destroyFramebuffer(mFramebuffer);
            mFramebuffer = VK_NULL_HANDLE;
        }
        mColorImage.reset();
        mDepthImage.reset();
    }

    void PickPass::createFramebuffer()
    {
        const std::array attachments = { mColorImage->GetImageView(),
                                         mDepthImage->GetImageView() };

        const auto info =
            vk::FramebufferCreateInfo()
                .setRenderPass(mRenderPass)
                .setAttachments(attachments)
                .setWidth(mExtent.width)
                .setHeight(mExtent.height)
                .setLayers(1);

        mFramebuffer = mDevice->Get().createFramebuffer(info);
    }

    void PickPass::Resize(const vk::Extent2D     extent,
                          const skr::Arc<Image>& colorImage,
                          const skr::Arc<Image>& depthImage)
    {
        if (extent.width == 0 || extent.height == 0)
        {
            return;
        }

        if (mExtent.width == extent.width && mExtent.height == extent.height &&
            mColorImage && mDepthImage && mFramebuffer)
        {
            return;
        }

        mDevice->Get().waitIdle();
        destroyFramebufferResources();

        mExtent     = extent;
        mColorImage = colorImage;
        mDepthImage = depthImage;
        createFramebuffer();
    }

    void PickPass::Render(const skr::Arc<CommandPool>&   commandPool,
                          const ProjectionUniformBuffer& projection,
                          const std::function<void()>&   drawScene)
    {
        if (!mFramebuffer || mExtent.width == 0 || mExtent.height == 0)
        {
            return;
        }

        mUniformBuffer->Copy(&projection, sizeof(ProjectionUniformBuffer));

        auto& commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::Pick);

        const auto clearColor =
            vk::ClearColorValue().setUint32({ kPickMissId, 0u, 0u, 0u });
        const auto clearDepth = vk::ClearDepthStencilValue().setDepth(
            mFreyaOptions->ReverseZ ? 0.0f : 1.0f);

        const std::array clearValues = {
            vk::ClearValue().setColor(clearColor),
            vk::ClearValue().setDepthStencil(clearDepth),
        };

        const auto beginInfo =
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass)
                .setFramebuffer(mFramebuffer)
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(mExtent))
                .setClearValues(clearValues);

        commandBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mPipelineLayout,
            0,
            1,
            &mDescriptorSet,
            0,
            nullptr);

        const auto viewport =
            vk::Viewport()
                .setX(0.0f)
                .setY(0.0f)
                .setWidth(static_cast<float>(mExtent.width))
                .setHeight(static_cast<float>(mExtent.height))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);
        commandBuffer.setViewport(0, 1, &viewport);

        const auto scissor =
            vk::Rect2D().setOffset({ 0, 0 }).setExtent(mExtent);
        commandBuffer.setScissor(0, 1, &scissor);

        drawScene();

        commandBuffer.endRenderPass();
        mDevice->EndDebugLabel(commandBuffer);
    }

    void PickPass::PushEntityId(const skr::Arc<CommandPool>& commandPool,
                                const std::uint32_t          entityId) const
    {
        commandPool->GetCommandBuffer().pushConstants(
            mPipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(std::uint32_t),
            &entityId);
    }

    void PickPass::CopyPixel(const skr::Arc<CommandPool>& commandPool,
                             const std::uint32_t          x,
                             const std::uint32_t          y)
    {
        if (!mColorImage || !mStagingBuffer || mExtent.width == 0 ||
            mExtent.height == 0)
        {
            return;
        }

        const auto clampedX = std::min(x, mExtent.width - 1);
        const auto clampedY = std::min(y, mExtent.height - 1);

        auto& commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::PickCopy);

        // Transition ID image from color-attachment (final layout of the pick
        // render pass: TransferSrcOptimal) — render pass already leaves it in
        // TransferSrcOptimal. Barrier from color write to transfer read.
        const auto toTransfer =
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(mColorImage->GetImage())
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1));

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eTransfer,
            vk::DependencyFlags {},
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toTransfer);

        const auto region =
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
                .setImageOffset(vk::Offset3D(
                    static_cast<std::int32_t>(clampedX),
                    static_cast<std::int32_t>(clampedY),
                    0))
                .setImageExtent(vk::Extent3D(1, 1, 1));

        commandBuffer.copyImageToBuffer(
            mColorImage->GetImage(),
            vk::ImageLayout::eTransferSrcOptimal,
            mStagingBuffer->Get(),
            1,
            &region);

        mDevice->EndDebugLabel(commandBuffer);
    }

    std::uint32_t PickPass::ReadPixel() const
    {
        if (!mStagingBuffer)
        {
            return kPickMissId;
        }

        void* mapped = mDevice->Get().mapMemory(
            mStagingBuffer->GetMemory(),
            0,
            sizeof(std::uint32_t),
            vk::MemoryMapFlagBits {});

        std::uint32_t value = kPickMissId;
        if (mapped)
        {
            value = *static_cast<const std::uint32_t*>(mapped);
            mDevice->Get().unmapMemory(mStagingBuffer->GetMemory());
        }
        return value;
    }

} // namespace FREYA_NAMESPACE
