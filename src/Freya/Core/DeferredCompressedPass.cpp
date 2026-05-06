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
        const Ref<Device>&                          device,
        const Ref<FreyaOptions>&                    freyaOptions,
        const Ref<Surface>&                         surface,
        const vk::RenderPass                        renderPass,
        const vk::PipelineLayout                    vertexPipelineLayout,
        const vk::PipelineLayout                    fullscreenPipelineLayout,
        const vk::Pipeline                          depthPrepassPipeline,
        const vk::Pipeline                          gbufferPipeline,
        const vk::Pipeline                          lightingPipeline,
        const vk::Pipeline                          translucentPipeline,
        const Ref<Buffer>&                          uniformBuffer,
        const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<vk::DescriptorSet>&       descriptorSets,
        const vk::DescriptorPool                    descriptorPool,
        const std::vector<Ref<Image>>&              gbufferImages,
        const Ref<Image>&                           emissiveImage,
        const Ref<Image>&                           depthImage,
        const Ref<Image>&                           translucentImage,
        const Ref<Image>&                           opaqueImage,
        const std::vector<vk::Framebuffer>&         framebuffers,
        const vk::DescriptorSetLayout               inputAttachmentLayout,
        const vk::DescriptorPool                    inputAttachmentPool,
        const vk::DescriptorSet                     lightingInputSet,
        const vk::DescriptorSetLayout               samplerLayout,
        const vk::DescriptorPool                    samplerDescriptorPool) :
        mDevice(device), mFreyaOptions(freyaOptions), mSurface(surface),
        mRenderPass(renderPass), mVertexPipelineLayout(vertexPipelineLayout),
        mFullscreenPipelineLayout(fullscreenPipelineLayout),
        mUniformBuffer(uniformBuffer),
        mDescriptorSetLayouts(descriptorSetLayouts),
        mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool),
        mGBufferImages(gbufferImages), mEmissiveImage(emissiveImage),
        mDepthImage(depthImage), mTranslucentImage(translucentImage),
        mOpaqueImage(opaqueImage), mFramebuffers(framebuffers),
        mInputAttachmentLayout(inputAttachmentLayout),
        mInputAttachmentPool(inputAttachmentPool),
        mLightingInputSet(lightingInputSet), mSamplerLayout(samplerLayout),
        mSamplerDescriptorPool(samplerDescriptorPool)
    {
        mPipelines[DefDepthPrePass]    = depthPrepassPipeline;
        mPipelines[DefGBufferPass]     = gbufferPipeline;
        mPipelines[DefLightingPass]    = lightingPipeline;
        mPipelines[DefTranslucentPass] = translucentPipeline;
    }

    DeferredCompressedPass::~DeferredCompressedPass()
    {
        auto& vkDevice = mDevice->Get();

        for (auto& fb : mFramebuffers)
            vkDevice.destroyFramebuffer(fb);

        vkDevice.destroyDescriptorPool(mInputAttachmentPool);
        vkDevice.destroyDescriptorSetLayout(mInputAttachmentLayout);

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
        return mPipelines[subpass];
    }

    void DeferredCompressedPass::Begin(
        const Ref<SwapChain>    swapChain,
        const Ref<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        beginDebugLabel(commandBuffer, "Gbuffer+Lighting Pass", mDevice->Get());

        // 8 clear values (no backbuffer — composite pass handles it)
        auto clearValues = std::vector<vk::ClearValue> {
            vk::ClearValue().setDepthStencil(
                vk::ClearDepthStencilValue().setDepth(
                    mFreyaOptions->ReverseZ ? 0.0f : 1.0f)),       // depth
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // position
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // normal
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // albedo
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // emissive
            vk::ClearValue().setColor(
                { 0.0f, 0.5f, 0.0f, 0.0f }), // material (metalness, roughness)
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // transl.
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // opaque
        };

        const auto imageIndex = swapChain->GetCurrentImageIndex();
        const auto frameIndex = swapChain->GetCurrentFrameIndex();

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass)
                .setFramebuffer(mFramebuffers[imageIndex])
                .setRenderArea(vk::Rect2D().setOffset({ 0, 0 }).setExtent(
                    swapChain->GetExtent()))
                .setClearValues(clearValues),
            vk::SubpassContents::eInline);

        mLabelActive = false;
        BindPipeline(DefDepthPrePass, commandPool, frameIndex);
    }

    void DeferredCompressedPass::NextSubpass(
        const Ref<CommandPool>& commandPool) const
    {
        commandPool->GetCommandBuffer().nextSubpass(
            vk::SubpassContents::eInline);
    }

    void DeferredCompressedPass::BindPipeline(
        const std::uint32_t     subpass,
        const Ref<CommandPool>& commandPool,
        const std::uint32_t     frameIndex) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        if (mLabelActive)
        {
            endDebugLabel(commandBuffer, mDevice->Get());
        }

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mPipelines[subpass]);

        beginDebugLabel(commandBuffer, GetSubpassLabel(subpass),
                        mDevice->Get());
        mLabelActive = true;

        if (subpass == DefDepthPrePass || subpass == DefGBufferPass ||
            subpass == DefTranslucentPass)
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

        if (subpass == DefLightingPass)
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
    }

    void DeferredCompressedPass::AdvanceSubpass(
        const std::uint32_t     subpass,
        const Ref<CommandPool>& commandPool,
        const std::uint32_t     frameIndex) const
    {
        NextSubpass(commandPool);
        BindPipeline(subpass, commandPool, frameIndex);
    }

    void DeferredCompressedPass::DrawFullscreenTriangle(
        const Ref<CommandPool>& commandPool) const
    {
        commandPool->GetCommandBuffer().draw(3, 1, 0, 0);
    }

    void DeferredCompressedPass::End(const Ref<CommandPool> commandPool) const
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
            case DefLightingPass:
                return "Lighting";
            case DefTranslucentPass:
                return "Translucent";
            default:
                return "Unknown";
        }
    }

} // namespace FREYA_NAMESPACE
