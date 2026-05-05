#include "DeferredCompressedPass.hpp"

namespace FREYA_NAMESPACE
{
    DeferredCompressedPass::DeferredCompressedPass(
        const Ref<Device>&               device,
        const Ref<FreyaOptions>&         freyaOptions,
        const Ref<Surface>&              surface,
        const vk::RenderPass             renderPass,
        const vk::PipelineLayout         vertexPipelineLayout,
        const vk::PipelineLayout         fullscreenPipelineLayout,
        const vk::Pipeline               depthPrepassPipeline,
        const vk::Pipeline               gbufferPipeline,
        const vk::Pipeline               lightingPipeline,
        const vk::Pipeline               translucentPipeline,
        const vk::Pipeline               compositePipeline,
        const Ref<Buffer>&               uniformBuffer,
        const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<vk::DescriptorSet>&       descriptorSets,
        const vk::DescriptorPool                    descriptorPool,
        const std::vector<Ref<Image>>&              gbufferImages,
        const Ref<Image>&                           depthImage,
        const Ref<Image>&                           translucentImage,
        const Ref<Image>&                           opaqueImage,
        const std::vector<vk::Framebuffer>&         framebuffers,
        const vk::DescriptorSetLayout               inputAttachmentLayout,
        const vk::DescriptorPool                    inputAttachmentPool,
        const vk::DescriptorSet                     lightingInputSet,
        const vk::DescriptorSet                     compositeInputSet,
        const vk::DescriptorSetLayout               samplerLayout,
        const vk::DescriptorPool                    samplerDescriptorPool) :
        mDevice(device),
        mFreyaOptions(freyaOptions),
        mSurface(surface),
        mRenderPass(renderPass),
        mVertexPipelineLayout(vertexPipelineLayout),
        mFullscreenPipelineLayout(fullscreenPipelineLayout),
        mUniformBuffer(uniformBuffer),
        mDescriptorSetLayouts(descriptorSetLayouts),
        mDescriptorSets(descriptorSets),
        mDescriptorPool(descriptorPool),
        mGBufferImages(gbufferImages),
        mDepthImage(depthImage),
        mTranslucentImage(translucentImage),
        mOpaqueImage(opaqueImage),
        mFramebuffers(framebuffers),
        mInputAttachmentLayout(inputAttachmentLayout),
        mInputAttachmentPool(inputAttachmentPool),
        mLightingInputSet(lightingInputSet),
        mCompositeInputSet(compositeInputSet),
        mSamplerLayout(samplerLayout),
        mSamplerDescriptorPool(samplerDescriptorPool)
    {
        mPipelines[DeferredDepthPrePass]    = depthPrepassPipeline;
        mPipelines[DeferredGBufferPass]     = gbufferPipeline;
        mPipelines[DeferredLightingPass]    = lightingPipeline;
        mPipelines[DeferredTranslucentPass] = translucentPipeline;
        mPipelines[DeferredCompositePass]   = compositePipeline;
    }

    DeferredCompressedPass::~DeferredCompressedPass()
    {
        auto& vkDevice = mDevice->Get();

        // Destroy framebuffers
        for (auto& fb : mFramebuffers)
        {
            vkDevice.destroyFramebuffer(fb);
        }

        // Destroy input attachment resources
        vkDevice.destroyDescriptorPool(mInputAttachmentPool);
        vkDevice.destroyDescriptorSetLayout(mInputAttachmentLayout);

        // Destroy sampler pool/layout
        vkDevice.destroyDescriptorPool(mSamplerDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mSamplerLayout);

        // Destroy descriptor pool
        vkDevice.destroyDescriptorPool(mDescriptorPool);

        // Destroy descriptor set layouts
        for (const auto& layout : mDescriptorSetLayouts)
        {
            vkDevice.destroyDescriptorSetLayout(layout);
        }

        // Destroy pipelines
        for (auto& pipeline : mPipelines)
        {
            vkDevice.destroyPipeline(pipeline);
        }

        // Destroy pipeline layouts
        vkDevice.destroyPipelineLayout(mVertexPipelineLayout);
        vkDevice.destroyPipelineLayout(mFullscreenPipelineLayout);

        // Destroy render pass
        vkDevice.destroyRenderPass(mRenderPass);

        // Destroy G-buffer and intermediate images (their Ref<> will release)
        mGBufferImages.clear();
        mDepthImage.reset();
        mTranslucentImage.reset();
        mOpaqueImage.reset();

        // Destroy uniform buffer
        mUniformBuffer.reset();
    }

    vk::Pipeline& DeferredCompressedPass::GetPipeline(
        const std::uint32_t subpass)
    {
        return mPipelines[subpass];
    }

    void DeferredCompressedPass::Begin(
        const Ref<SwapChain>  swapChain,
        const Ref<CommandPool> commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        // All 7 attachments need clear values.
        // Depth: reverse-Z clears to 0.0 (far plane).
        auto clearValues = std::vector<vk::ClearValue> {
            vk::ClearValue().setColor(mFreyaOptions->clearColor), // backbuffer
            vk::ClearValue().setDepthStencil(
                vk::ClearDepthStencilValue().setDepth(
                    mFreyaOptions->ReverseZ ? 0.0f : 1.0f)), // depth
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // position
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // normal
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // albedo
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // transl.
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f })  // opaque
        };

        // Framebuffer must be selected by the acquired swapchain image index
        // (not the ring-buffer frame index) so we render to the same image
        // that will be presented.
        const auto imageIndex = swapChain->GetCurrentImageIndex();
        const auto frameIndex = swapChain->GetCurrentFrameIndex();

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass)
                .setFramebuffer(mFramebuffers[imageIndex])
                .setRenderArea(
                    vk::Rect2D()
                        .setOffset({ 0, 0 })
                        .setExtent(swapChain->GetExtent()))
                .setClearValues(clearValues),
            vk::SubpassContents::eInline);

        // We are now in subpass 0 (depth pre-pass).
        BindPipeline(DeferredDepthPrePass, commandPool, frameIndex);
    }

    void DeferredCompressedPass::NextSubpass(
        const Ref<CommandPool> commandPool) const
    {
        commandPool->GetCommandBuffer().nextSubpass(
            vk::SubpassContents::eInline);
    }

    void DeferredCompressedPass::BindPipeline(
        const std::uint32_t   subpass,
        const Ref<CommandPool>& commandPool,
        const std::uint32_t   frameIndex) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mPipelines[subpass]);

        // Bind UBO descriptor set for subpasses that use it
        // (depth, gbuffer, translucent all use UBO at set 0 binding 0)
        if (subpass == DeferredDepthPrePass ||
            subpass == DeferredGBufferPass ||
            subpass == DeferredTranslucentPass)
        {
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mVertexPipelineLayout,
                0,
                1,
                &mDescriptorSets[frameIndex],
                0,
                nullptr);
        }

        // Bind input attachment descriptor set for lighting subpass
        if (subpass == DeferredLightingPass)
        {
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mFullscreenPipelineLayout,
                0,
                1,
                &mLightingInputSet,
                0,
                nullptr);
        }

        // Bind input attachment descriptor set for composite subpass
        if (subpass == DeferredCompositePass)
        {
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mFullscreenPipelineLayout,
                0,
                1,
                &mCompositeInputSet,
                0,
                nullptr);
        }
    }

    void DeferredCompressedPass::AdvanceSubpass(
        const std::uint32_t   subpass,
        const Ref<CommandPool>& commandPool,
        const std::uint32_t   frameIndex) const
    {
        NextSubpass(commandPool);
        BindPipeline(subpass, commandPool, frameIndex);
    }

    void DeferredCompressedPass::DrawFullscreenTriangle(
        const Ref<CommandPool>& commandPool) const
    {
        commandPool->GetCommandBuffer().draw(3, 1, 0, 0);
    }

    void DeferredCompressedPass::End(
        const Ref<CommandPool> commandPool) const
    {
        commandPool->GetCommandBuffer().endRenderPass();
    }

    void DeferredCompressedPass::UpdateProjection(
        const ProjectionUniformBuffer& buffer,
        const std::uint32_t            frameIndex) const
    {
        const auto offset = frameIndex * sizeof(ProjectionUniformBuffer);
        mUniformBuffer->Copy(&buffer, sizeof(ProjectionUniformBuffer), offset);

        auto bufferInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mUniformBuffer->Get())
                .setOffset(offset)
                .setRange(sizeof(ProjectionUniformBuffer));

        const auto descriptorWriter =
            vk::WriteDescriptorSet()
                .setDstSet(mDescriptorSets[frameIndex])
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(bufferInfo);

        mDevice->Get().updateDescriptorSets(1, &descriptorWriter, 0, nullptr);
    }

} // namespace FREYA_NAMESPACE
