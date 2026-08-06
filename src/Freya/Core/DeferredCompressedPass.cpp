#include "DeferredCompressedPass.hpp"

#include <glm/gtc/matrix_inverse.hpp>

namespace FREYA_NAMESPACE
{
    DeferredCompressedPass::DeferredCompressedPass(
        const skr::Arc<Device>&                     device,
        const skr::Arc<FreyaOptions>&               freyaOptions,
        const skr::Arc<Surface>&                    surface,
        const vk::RenderPass                        renderPass,
        const vk::PipelineLayout                    vertexPipelineLayout,
        const vk::PipelineLayout                    fullscreenPipelineLayout,
        const vk::Pipeline                          depthPrepassPipeline,
        const vk::Pipeline                          gbufferPipeline,
        const vk::Pipeline                          lightingPipeline,
        const skr::Arc<Buffer>&                     uniformBuffer,
        const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<vk::DescriptorSet>&       descriptorSets,
        const vk::DescriptorPool                    descriptorPool,
        const std::vector<skr::Arc<Image>>&         gbufferImages,
        const skr::Arc<Image>&                      sceneColorImage,
        const skr::Arc<Image>&                      velocityImage,
        const skr::Arc<Image>&                      depthImage,
        const skr::Arc<Image>&                      translucentImage,
        const std::vector<vk::Framebuffer>&         framebuffers,
        const vk::DescriptorSetLayout               lightingSetLayout,
        const vk::DescriptorPool                    lightingDescriptorPool,
        const std::vector<vk::DescriptorSet>&       lightingSets,
        const vk::DescriptorSetLayout               samplerLayout,
        const vk::DescriptorPool                    samplerDescriptorPool,
        const vk::Sampler                           gbufferSampler,
        const vk::Extent2D                          extent) :
        mDevice(device), mFreyaOptions(freyaOptions), mSurface(surface),
        mRenderPass(renderPass), mVertexPipelineLayout(vertexPipelineLayout),
        mFullscreenPipelineLayout(fullscreenPipelineLayout),
        mUniformBuffer(uniformBuffer),
        mDescriptorSetLayouts(descriptorSetLayouts),
        mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool),
        mGBufferImages(gbufferImages), mSceneColorImage(sceneColorImage),
        mVelocityImage(velocityImage), mDepthImage(depthImage),
        mTranslucentImage(translucentImage), mFramebuffers(framebuffers),
        mExtent(extent), mLightingSetLayout(lightingSetLayout),
        mLightingDescriptorPool(lightingDescriptorPool),
        mLightingSets(lightingSets), mSamplerLayout(samplerLayout),
        mSamplerDescriptorPool(samplerDescriptorPool),
        mGbufferSampler(gbufferSampler)
    {
        mPipelines[DefDepthPrePass] = depthPrepassPipeline;
        mPipelines[DefGBufferPass]  = gbufferPipeline;
        mPipelines[DefLightingPass] = lightingPipeline;
    }

    DeferredCompressedPass::~DeferredCompressedPass()
    {
        auto& vkDevice = mDevice->Get();

        for (auto& fb : mFramebuffers)
            vkDevice.destroyFramebuffer(fb);

        vkDevice.destroyDescriptorPool(mLightingDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mLightingSetLayout);

        vkDevice.destroyDescriptorPool(mSamplerDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mSamplerLayout);

        vkDevice.destroyDescriptorPool(mDescriptorPool);

        for (const auto& layout : mDescriptorSetLayouts)
            vkDevice.destroyDescriptorSetLayout(layout);

        for (auto& pipeline : mPipelines)
            vkDevice.destroyPipeline(pipeline);

        vkDevice.destroyPipelineLayout(mVertexPipelineLayout);
        vkDevice.destroyPipelineLayout(mFullscreenPipelineLayout);

        vkDevice.destroyRenderPass(mRenderPass);

        vkDevice.destroySampler(mGbufferSampler);

        mGBufferImages.clear();
        mDepthImage.reset();
        mSceneColorImage.reset();
        mVelocityImage.reset();
        mTranslucentImage.reset();

        mUniformBuffer.reset();
    }

    vk::Pipeline& DeferredCompressedPass::GetPipeline(
        const std::uint32_t subpass)
    {
        return mPipelines[subpass];
    }

    void DeferredCompressedPass::Begin(
        const skr::Arc<SwapChain>    swapChain,
        const skr::Arc<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        // Keep composite translucent input defined (unused by this pass).
        {
            auto range =
                vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1);

            auto toTransfer =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eUndefined)
                    .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setSrcAccessMask({})
                    .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
                    .setImage(mTranslucentImage->GetImage())
                    .setSubresourceRange(range);
            commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                          vk::PipelineStageFlagBits::eTransfer,
                                          {}, nullptr, nullptr, toTransfer);

            commandBuffer.clearColorImage(
                mTranslucentImage->GetImage(),
                vk::ImageLayout::eTransferDstOptimal,
                vk::ClearColorValue(std::array { 0.f, 0.f, 0.f, 0.f }), range);

            auto toSampled =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                    .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                    .setImage(mTranslucentImage->GetImage())
                    .setSubresourceRange(range);
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr,
                nullptr, toSampled);
        }

        mDevice->BeginDebugLabel(commandBuffer, "Deferred");

        auto clearValues = std::vector<vk::ClearValue> {
            vk::ClearValue().setDepthStencil(
                vk::ClearDepthStencilValue().setDepth(
                    mFreyaOptions->ReverseZ ? 0.0f : 1.0f)),
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // albedo
            vk::ClearValue().setColor(
                { 0.5f, 0.5f, 0.5f, 1.0f / 3.0f }), // normal + receiveShadow
            vk::ClearValue().setColor(
                { 0.5f, 0.0f, 1.0f, 0.0f }), // rough, metal, AO, free
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // scene HDR
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // velocity
        };

        const auto imageIndex = swapChain->GetCurrentImageIndex();
        const auto frameIndex = swapChain->GetCurrentFrameIndex();

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass)
                .setFramebuffer(mFramebuffers[imageIndex])
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(mExtent))
                .setClearValues(clearValues),
            vk::SubpassContents::eInline);

        mLabelActive    = false;
        mCurrentSubpass = DefDepthPrePass;
        BindPipeline(DefDepthPrePass, commandPool, frameIndex);
    }

    void DeferredCompressedPass::NextSubpass(
        const skr::Arc<CommandPool>& commandPool) const
    {
        commandPool->GetCommandBuffer().nextSubpass(
            vk::SubpassContents::eInline);
    }

    void DeferredCompressedPass::BindPipeline(
        const std::uint32_t          subpass,
        const skr::Arc<CommandPool>& commandPool,
        const std::uint32_t          frameIndex) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        if (mLabelActive)
        {
            mDevice->EndDebugLabel(commandBuffer);
        }

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mPipelines[subpass]);

        mDevice->BeginDebugLabel(commandBuffer, GetSubpassLabel(subpass));
        mLabelActive    = true;
        mCurrentSubpass = subpass;

        if (subpass == DefLightingPass)
        {
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mFullscreenPipelineLayout,
                0,
                1,
                &mLightingSets[frameIndex],
                0,
                nullptr);
        }
        else
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
    }

    void DeferredCompressedPass::AdvanceSubpass(
        const std::uint32_t          subpass,
        const skr::Arc<CommandPool>& commandPool,
        const std::uint32_t          frameIndex) const
    {
        NextSubpass(commandPool);
        BindPipeline(subpass, commandPool, frameIndex);
    }

    void DeferredCompressedPass::DrawLighting(
        const skr::Arc<CommandPool>& commandPool, const std::uint32_t) const
    {
        commandPool->GetCommandBuffer().draw(3, 1, 0, 0);
    }

    void DeferredCompressedPass::End(
        const skr::Arc<CommandPool> commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        if (mLabelActive)
        {
            mDevice->EndDebugLabel(commandBuffer);
            mLabelActive = false;
        }

        commandBuffer.endRenderPass();
        mDevice->EndDebugLabel(commandBuffer);
    }

    void DeferredCompressedPass::UpdateProjection(
        const ProjectionUniformBuffer& buffer,
        const std::uint32_t            frameIndex) const
    {
        auto upload = buffer;
        upload.invViewProjection =
            glm::inverse(buffer.projection * buffer.view);

        const auto offset = frameIndex * sizeof(ProjectionUniformBuffer);
        mUniformBuffer->Copy(&upload, sizeof(ProjectionUniformBuffer), offset);
    }

    const char* DeferredCompressedPass::GetSubpassLabel(
        const std::uint32_t subpass)
    {
        switch (subpass)
        {
            case DefDepthPrePass:
                return "Depth Pre-pass";
            case DefGBufferPass:
                return "G-buffer";
            case DefLightingPass:
                return "Lighting";
            default:
                return "Unknown";
        }
    }

} // namespace FREYA_NAMESPACE
