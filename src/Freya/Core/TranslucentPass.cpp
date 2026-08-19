#include "Freya/Core/TranslucentPass.hpp"

#include "Freya/Core/DebugLabels.hpp"
#include "Freya/Internal/LightServiceGpu.hpp"

#include <array>
#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>

namespace FREYA_NAMESPACE
{
    TranslucentPass::TranslucentPass(
        const skr::Arc<Device>&                      device,
        const skr::Arc<FreyaOptions>&                freyaOptions,
        const skr::Arc<MaterialDescriptorResources>& materialResources,
        const skr::Arc<BoneMatrixResources>&         boneResources,
        const skr::Arc<LightService>&                lightService,
        const vk::RenderPass                         accumulateRenderPass,
        const vk::RenderPass                         resolveRenderPass,
        const vk::PipelineLayout                     accumulateLayout,
        const vk::PipelineLayout                     resolveLayout,
        const vk::Pipeline                           accumulatePipeline,
        const vk::Pipeline                           resolvePipeline,
        const skr::Arc<Buffer>&                      uniformBuffer,
        const vk::DescriptorSetLayout                cameraSetLayout,
        const vk::DescriptorPool                     cameraDescriptorPool,
        const std::vector<vk::DescriptorSet>&        cameraSets,
        const vk::DescriptorSetLayout                resolveSetLayout,
        const vk::DescriptorPool                     resolveDescriptorPool,
        const std::vector<vk::DescriptorSet>&        resolveSets,
        std::vector<skr::Arc<Image>>
            oitAccum,
        std::vector<skr::Arc<Image>>
            oitReveal,
        std::vector<skr::Arc<Image>>
            sceneWithTranslucency,
        std::vector<vk::Framebuffer>
            accumulateFramebuffers,
        std::vector<vk::Framebuffer>
                           resolveFramebuffers,
        const vk::Sampler  sampler,
        const vk::Format   depthFormat,
        const vk::Extent2D extent) :
        mDevice(device), mFreyaOptions(freyaOptions),
        mMaterialResources(materialResources), mBoneResources(boneResources),
        mLightService(lightService),
        mAccumulateRenderPass(accumulateRenderPass),
        mResolveRenderPass(resolveRenderPass),
        mAccumulateLayout(accumulateLayout), mResolveLayout(resolveLayout),
        mAccumulatePipeline(accumulatePipeline),
        mResolvePipeline(resolvePipeline), mUniformBuffer(uniformBuffer),
        mCameraSetLayout(cameraSetLayout),
        mCameraDescriptorPool(cameraDescriptorPool), mCameraSets(cameraSets),
        mResolveSetLayout(resolveSetLayout),
        mResolveDescriptorPool(resolveDescriptorPool),
        mResolveSets(resolveSets), mOitAccum(std::move(oitAccum)),
        mOitReveal(std::move(oitReveal)),
        mSceneWithTranslucency(std::move(sceneWithTranslucency)),
        mAccumulateFramebuffers(std::move(accumulateFramebuffers)),
        mResolveFramebuffers(std::move(resolveFramebuffers)), mSampler(sampler),
        mDepthFormat(depthFormat), mExtent(extent),
        mBoundOpaqueViews(resolveSets.size())
    {
    }

    TranslucentPass::~TranslucentPass()
    {
        auto& vkDevice = mDevice->Get();

        for (auto fb : mAccumulateFramebuffers)
            vkDevice.destroyFramebuffer(fb);
        for (auto fb : mResolveFramebuffers)
            vkDevice.destroyFramebuffer(fb);
        vkDevice.destroyPipeline(mAccumulatePipeline);
        vkDevice.destroyPipeline(mResolvePipeline);
        vkDevice.destroyPipelineLayout(mAccumulateLayout);
        vkDevice.destroyPipelineLayout(mResolveLayout);
        vkDevice.destroyRenderPass(mAccumulateRenderPass);
        vkDevice.destroyRenderPass(mResolveRenderPass);
        vkDevice.destroyDescriptorPool(mCameraDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mCameraSetLayout);
        vkDevice.destroyDescriptorPool(mResolveDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mResolveSetLayout);
        vkDevice.destroySampler(mSampler);

        mOitAccum.clear();
        mOitReveal.clear();
        mSceneWithTranslucency.clear();
        mUniformBuffer.reset();
        mMaterialResources.reset();
        mBoneResources.reset();
        mLightService.reset();
    }

    void TranslucentPass::UpdateProjection(
        const ProjectionUniformBuffer& buffer,
        const std::uint32_t            frameIndex) const
    {
        auto upload = buffer;
        upload.invViewProjection =
            glm::inverse(buffer.projection * buffer.view);
        const auto offset = frameIndex * sizeof(ProjectionUniformBuffer);
        mUniformBuffer->Copy(&upload, sizeof(ProjectionUniformBuffer), offset);
    }

    void TranslucentPass::BeginAccumulate(
        const skr::Arc<CommandPool>& commandPool,
        const std::uint32_t          frameIndex) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::Translucent);

        if (frameIndex >= mAccumulateFramebuffers.size())
            return;

        auto clears = std::array {
            vk::ClearValue().setColor(
                vk::ClearColorValue(std::array { 0.f, 0.f, 0.f, 0.f })),
            vk::ClearValue().setColor(
                vk::ClearColorValue(std::array { 1.f, 0.f, 0.f, 0.f })),
            vk::ClearValue().setDepthStencil(
                vk::ClearDepthStencilValue().setDepth(0.f)),
        };

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mAccumulateRenderPass)
                .setFramebuffer(mAccumulateFramebuffers[frameIndex])
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(mExtent))
                .setClearValues(clears),
            vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mAccumulatePipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, mAccumulateLayout, 0, 1,
            &mCameraSets[frameIndex], 0, nullptr);

        if (mLightService)
        {
            auto lightSet = LightServiceGpu::Set(*mLightService, frameIndex);
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, mAccumulateLayout, 2, 1,
                &lightSet, 0, nullptr);
        }

        if (mBoneResources)
        {
            auto boneSet = mBoneResources->GetSet(frameIndex);
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, mAccumulateLayout, 3, 1,
                &boneSet, 0, nullptr);
        }
    }

    void TranslucentPass::EndAccumulate(
        const skr::Arc<CommandPool>& commandPool) const
    {
        commandPool->GetCommandBuffer().endRenderPass();
    }

    void TranslucentPass::Resolve(const skr::Arc<CommandPool>& commandPool,
                                  const skr::Arc<Image>&       opaqueImage,
                                  const std::uint32_t          frameIndex) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        if (frameIndex >= mResolveSets.size() ||
            frameIndex >= mResolveFramebuffers.size())
            return;

        const auto opaqueView = opaqueImage->GetImageView();
        if (mBoundOpaqueViews[frameIndex] != opaqueView)
        {
            mBoundOpaqueViews[frameIndex] = opaqueView;
            auto opaqueInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mSampler)
                    .setImageView(opaqueView)
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            auto writer =
                vk::WriteDescriptorSet()
                    .setDstSet(mResolveSets[frameIndex])
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(opaqueInfo);
            mDevice->Get().updateDescriptorSets(1, &writer, 0, nullptr);
        }

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mResolveRenderPass)
                .setFramebuffer(mResolveFramebuffers[frameIndex])
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(mExtent))
                .setClearValueCount(0),
            vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mResolvePipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, mResolveLayout, 0, 1,
            &mResolveSets[frameIndex], 0, nullptr);
        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();

        mDevice->EndDebugLabel(commandBuffer);
    }

} // namespace FREYA_NAMESPACE
