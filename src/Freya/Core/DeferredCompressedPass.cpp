#include "DeferredCompressedPass.hpp"

#include <vulkan/vulkan.h>

namespace
{
    void beginDebugLabel(const vk::CommandBuffer& cmd,
                         const char*              name,
                         const vk::Device&        device)
    {
        auto func = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            device.getProcAddr("vkCmdBeginDebugUtilsLabelEXT"));
        if (!func)
            return;

        VkDebugUtilsLabelEXT label {};
        label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        func(static_cast<VkCommandBuffer>(cmd), &label);
    }

    void endDebugLabel(const vk::CommandBuffer& cmd, const vk::Device& device)
    {
        auto func = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            device.getProcAddr("vkCmdEndDebugUtilsLabelEXT"));
        if (!func)
            return;

        func(static_cast<VkCommandBuffer>(cmd));
    }
} // anonymous namespace

namespace FREYA_NAMESPACE
{
    DeferredCompressedPass::DeferredCompressedPass(
        const skr::Arc<Device>&                     device,
        const skr::Arc<FreyaOptions>&               freyaOptions,
        const skr::Arc<Surface>&                    surface,
        const vk::RenderPass                        gbufferRenderPass,
        const vk::RenderPass                        lightingRenderPass,
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
        const skr::Arc<Image>&                      emissiveImage,
        const skr::Arc<Image>&                      depthImage,
        const skr::Arc<Image>&                      translucentImage,
        const skr::Arc<Image>&                      opaqueImage,
        const std::vector<vk::Framebuffer>&         gbufferFramebuffers,
        const std::vector<vk::Framebuffer>&         lightingFramebuffers,
        const vk::DescriptorSetLayout               lightingSetLayout,
        const vk::DescriptorPool                    lightingDescriptorPool,
        const std::vector<vk::DescriptorSet>&       lightingSets,
        const vk::DescriptorSetLayout               samplerLayout,
        const vk::DescriptorPool                    samplerDescriptorPool,
        const vk::Sampler                           gbufferSampler,
        const vk::Extent2D                          extent) :
        mDevice(device), mFreyaOptions(freyaOptions), mSurface(surface),
        mGbufferRenderPass(gbufferRenderPass),
        mLightingRenderPass(lightingRenderPass),
        mVertexPipelineLayout(vertexPipelineLayout),
        mFullscreenPipelineLayout(fullscreenPipelineLayout),
        mLightingPipeline(lightingPipeline), mUniformBuffer(uniformBuffer),
        mDescriptorSetLayouts(descriptorSetLayouts),
        mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool),
        mGBufferImages(gbufferImages), mEmissiveImage(emissiveImage),
        mDepthImage(depthImage), mTranslucentImage(translucentImage),
        mOpaqueImage(opaqueImage), mGbufferFramebuffers(gbufferFramebuffers),
        mLightingFramebuffers(lightingFramebuffers), mExtent(extent),
        mLightingSetLayout(lightingSetLayout),
        mLightingDescriptorPool(lightingDescriptorPool),
        mLightingSets(lightingSets), mSamplerLayout(samplerLayout),
        mSamplerDescriptorPool(samplerDescriptorPool),
        mGbufferSampler(gbufferSampler)
    {
        mGeometryPipelines[DefDepthPrePass] = depthPrepassPipeline;
        mGeometryPipelines[DefGBufferPass]  = gbufferPipeline;
    }

    DeferredCompressedPass::~DeferredCompressedPass()
    {
        auto& vkDevice = mDevice->Get();

        for (auto& fb : mGbufferFramebuffers)
            vkDevice.destroyFramebuffer(fb);
        for (auto& fb : mLightingFramebuffers)
            vkDevice.destroyFramebuffer(fb);

        vkDevice.destroyDescriptorPool(mLightingDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mLightingSetLayout);

        vkDevice.destroyDescriptorPool(mSamplerDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mSamplerLayout);

        vkDevice.destroyDescriptorPool(mDescriptorPool);

        for (const auto& layout : mDescriptorSetLayouts)
            vkDevice.destroyDescriptorSetLayout(layout);

        for (auto& pipeline : mGeometryPipelines)
            vkDevice.destroyPipeline(pipeline);
        vkDevice.destroyPipeline(mLightingPipeline);

        vkDevice.destroyPipelineLayout(mVertexPipelineLayout);
        vkDevice.destroyPipelineLayout(mFullscreenPipelineLayout);

        vkDevice.destroyRenderPass(mGbufferRenderPass);
        vkDevice.destroyRenderPass(mLightingRenderPass);

        vkDevice.destroySampler(mGbufferSampler);

        mGBufferImages.clear();
        mDepthImage.reset();
        mEmissiveImage.reset();
        mTranslucentImage.reset();
        mOpaqueImage.reset();

        mUniformBuffer.reset();
    }

    vk::Pipeline& DeferredCompressedPass::GetPipeline(
        const std::uint32_t subpass)
    {
        return mGeometryPipelines[subpass];
    }

    void DeferredCompressedPass::Begin(
        const skr::Arc<SwapChain>    swapChain,
        const skr::Arc<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        beginDebugLabel(commandBuffer, "Deferred G-buffer", mDevice->Get());

        auto clearValues = std::vector<vk::ClearValue> {
            vk::ClearValue().setDepthStencil(
                vk::ClearDepthStencilValue().setDepth(
                    mFreyaOptions->ReverseZ ? 0.0f : 1.0f)),
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }),
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }),
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }),
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }),
            vk::ClearValue().setColor({ 0.0f, 0.5f, 0.0f, 0.0f }),
        };

        const auto imageIndex = swapChain->GetCurrentImageIndex();
        const auto frameIndex = swapChain->GetCurrentFrameIndex();

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mGbufferRenderPass)
                .setFramebuffer(mGbufferFramebuffers[imageIndex])
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
            endDebugLabel(commandBuffer, mDevice->Get());
        }

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mGeometryPipelines[subpass]);

        beginDebugLabel(commandBuffer, GetSubpassLabel(subpass),
                        mDevice->Get());
        mLabelActive    = true;
        mCurrentSubpass = subpass;

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mVertexPipelineLayout,
            0,
            1,
            &mDescriptorSets[frameIndex],
            0,
            nullptr);
    }

    void DeferredCompressedPass::AdvanceSubpass(
        const std::uint32_t          subpass,
        const skr::Arc<CommandPool>& commandPool,
        const std::uint32_t          frameIndex) const
    {
        NextSubpass(commandPool);
        BindPipeline(subpass, commandPool, frameIndex);
    }

    void DeferredCompressedPass::End(
        const skr::Arc<CommandPool> commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        if (mLabelActive)
        {
            endDebugLabel(commandBuffer, mDevice->Get());
            mLabelActive = false;
        }

        commandBuffer.endRenderPass();
        endDebugLabel(commandBuffer, mDevice->Get());
    }

    void DeferredCompressedPass::BeginLighting(
        const skr::Arc<SwapChain>    swapChain,
        const skr::Arc<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        beginDebugLabel(commandBuffer, "Deferred Lighting", mDevice->Get());

        auto clearValues = std::vector<vk::ClearValue> {
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }),
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }),
        };

        const auto imageIndex = swapChain->GetCurrentImageIndex();
        const auto frameIndex = swapChain->GetCurrentFrameIndex();

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mLightingRenderPass)
                .setFramebuffer(mLightingFramebuffers[imageIndex])
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(mExtent))
                .setClearValues(clearValues),
            vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mLightingPipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mFullscreenPipelineLayout,
            0,
            1,
            &mLightingSets[frameIndex],
            0,
            nullptr);
    }

    void DeferredCompressedPass::DrawLighting(
        const skr::Arc<CommandPool>& commandPool, const std::uint32_t) const
    {
        commandPool->GetCommandBuffer().draw(3, 1, 0, 0);
    }

    void DeferredCompressedPass::EndLighting(
        const skr::Arc<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        commandBuffer.endRenderPass();
        endDebugLabel(commandBuffer, mDevice->Get());
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

    const char* DeferredCompressedPass::GetSubpassLabel(
        const std::uint32_t subpass)
    {
        switch (subpass)
        {
            case DefDepthPrePass:
                return "Depth Pre-pass";
            case DefGBufferPass:
                return "G-buffer";
            default:
                return "Unknown";
        }
    }

} // namespace FREYA_NAMESPACE
