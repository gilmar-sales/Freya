#include "DeferredCompressedPass.hpp"

#include <glm/gtc/matrix_inverse.hpp>

namespace FREYA_NAMESPACE
{
    DeferredCompressedPass::DeferredCompressedPass(
        const skr::Arc<Device>&                      device,
        const skr::Arc<FreyaOptions>&                freyaOptions,
        const skr::Arc<Surface>&                     surface,
        const vk::RenderPass                         renderPass,
        const vk::PipelineLayout                     vertexPipelineLayout,
        const vk::PipelineLayout                     fullscreenPipelineLayout,
        const vk::Pipeline                           depthPrepassPipeline,
        const vk::Pipeline                           gbufferPipeline,
        const vk::Pipeline                           lightingPipeline,
        const skr::Arc<Buffer>&                      uniformBuffer,
        const std::vector<vk::DescriptorSetLayout>&  descriptorSetLayouts,
        const std::vector<vk::DescriptorSet>&        descriptorSets,
        const vk::DescriptorPool                     descriptorPool,
        const std::vector<skr::Arc<Image>>&          gbufferImages,
        const skr::Arc<Image>&                       sceneColorImage,
        const skr::Arc<Image>&                       velocityImage,
        const skr::Arc<Image>&                       depthImage,
        const std::vector<vk::Framebuffer>&          framebuffers,
        const vk::RenderPass                         lightingRenderPass,
        const vk::Framebuffer                        lightingFramebuffer,
        const vk::DescriptorSetLayout                lightingSetLayout,
        const vk::DescriptorPool                     lightingDescriptorPool,
        const std::vector<vk::DescriptorSet>&        lightingSets,
        const skr::Arc<MaterialDescriptorResources>& materialResources,
        const skr::Arc<BoneMatrixResources>&         boneResources,
        const vk::Sampler                            gbufferSampler,
        const vk::Extent2D                           extent) :
        mDevice(device), mFreyaOptions(freyaOptions), mSurface(surface),
        mRenderPass(renderPass), mVertexPipelineLayout(vertexPipelineLayout),
        mFullscreenPipelineLayout(fullscreenPipelineLayout),
        mUniformBuffer(uniformBuffer),
        mDescriptorSetLayouts(descriptorSetLayouts),
        mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool),
        mGBufferImages(gbufferImages), mSceneColorImage(sceneColorImage),
        mVelocityImage(velocityImage), mDepthImage(depthImage),
        mFramebuffers(framebuffers), mLightingFramebuffer(lightingFramebuffer),
        mExtent(extent), mLightingRenderPass(lightingRenderPass),
        mLightingSetLayout(lightingSetLayout),
        mLightingDescriptorPool(lightingDescriptorPool),
        mLightingSets(lightingSets), mMaterialResources(materialResources),
        mBoneResources(boneResources), mGbufferSampler(gbufferSampler),
        mBoundSsaoViews(lightingSets.size())
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

        vkDevice.destroyFramebuffer(mLightingFramebuffer);

        vkDevice.destroyDescriptorPool(mLightingDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mLightingSetLayout);

        vkDevice.destroyDescriptorPool(mDescriptorPool);

        for (const auto& layout : mDescriptorSetLayouts)
            vkDevice.destroyDescriptorSetLayout(layout);

        for (auto& pipeline : mPipelines)
            vkDevice.destroyPipeline(pipeline);

        vkDevice.destroyPipelineLayout(mVertexPipelineLayout);
        vkDevice.destroyPipelineLayout(mFullscreenPipelineLayout);

        vkDevice.destroyRenderPass(mRenderPass);
        vkDevice.destroyRenderPass(mLightingRenderPass);

        vkDevice.destroySampler(mGbufferSampler);

        mGBufferImages.clear();
        mDepthImage.reset();
        mSceneColorImage.reset();
        mVelocityImage.reset();
        mMaterialResources.reset();

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

        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::DeferredGeometry);

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

        // Lighting uses a single pass label from BeginLighting().
        if (subpass != DefLightingPass)
        {
            mDevice->BeginDebugLabel(commandBuffer, GetSubpassRegion(subpass));
            mLabelActive = true;
        }
        else
        {
            mLabelActive = false;
        }
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
            if (mMaterialResources)
            {
                commandBuffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mFullscreenPipelineLayout,
                    1,
                    1,
                    &mMaterialResources->GetBindlessSet(),
                    0,
                    nullptr);
            }
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
            if (mBoneResources)
            {
                auto boneSet = mBoneResources->GetSet(frameIndex);
                commandBuffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mVertexPipelineLayout,
                    2,
                    1,
                    &boneSet,
                    0,
                    nullptr);
            }
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
        const skr::Arc<CommandPool>& commandPool, const std::uint32_t,
        const std::uint32_t          lightingDebug) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        struct LightingPush
        {
            std::uint32_t debugMode;
            std::uint32_t pad0;
            std::uint32_t pad1;
            std::uint32_t pad2;
        } push { lightingDebug, 0u, 0u, 0u };

        commandBuffer.pushConstants(
            mFullscreenPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(LightingPush), &push);
        commandBuffer.draw(3, 1, 0, 0);
    }

    void DeferredCompressedPass::BeginLighting(
        const skr::Arc<CommandPool>& commandPool,
        const skr::Arc<Image>&       ssaoImage,
        const std::uint32_t          frameIndex) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        const auto ssaoView = ssaoImage->GetImageView();
        if (frameIndex < mLightingSets.size() &&
            (frameIndex >= mBoundSsaoViews.size() ||
             mBoundSsaoViews[frameIndex] != ssaoView))
        {
            if (frameIndex >= mBoundSsaoViews.size())
                mBoundSsaoViews.resize(mLightingSets.size());

            const auto ssaoInfo =
                vk::DescriptorImageInfo {}
                    .setSampler(mGbufferSampler)
                    .setImageView(ssaoView)
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            const auto ssaoWrite =
                vk::WriteDescriptorSet {}
                    .setDstSet(mLightingSets[frameIndex])
                    .setDstBinding(15)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setImageInfo(ssaoInfo);
            mDevice->Get().updateDescriptorSets(ssaoWrite, nullptr);
            mBoundSsaoViews[frameIndex] = ssaoView;
        }

        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::DeferredLighting);

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mLightingRenderPass)
                .setFramebuffer(mLightingFramebuffer)
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(mExtent)),
            vk::SubpassContents::eInline);

        mLightingActive = true;
        BindPipeline(DefLightingPass, commandPool, frameIndex);
    }

    void DeferredCompressedPass::EndLighting(
        const skr::Arc<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        if (mLabelActive)
        {
            mDevice->EndDebugLabel(commandBuffer);
            mLabelActive = false;
        }

        commandBuffer.endRenderPass();
        mDevice->EndDebugLabel(commandBuffer);
        mLightingActive = false;
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
        return GetSubpassRegion(subpass).name;
    }

    DebugRegion DeferredCompressedPass::GetSubpassRegion(
        const std::uint32_t subpass)
    {
        switch (subpass)
        {
            case DefDepthPrePass:
                return DebugLabel::DepthPrePass;
            case DefGBufferPass:
                return DebugLabel::GBuffer;
            case DefLightingPass:
                return DebugLabel::DeferredLighting;
            default:
                return { "Unknown", DebugLabel::GeometryColor };
        }
    }

} // namespace FREYA_NAMESPACE
