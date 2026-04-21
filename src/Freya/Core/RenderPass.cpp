#include "RenderPass.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Destroys all Vulkan resources: descriptor pools, pipeline, layout,
     * render pass.
     */
    RenderPass::~RenderPass()
    {
        mDevice->Get().destroyDescriptorPool(mSamplerDescriptorPool);

        mDevice->Get().destroyDescriptorPool(mDescriptorPool);

        mUniformBuffer.reset();

        for (const auto& descriptorSetLayout : mDescriptorSetLayouts)
        {
            mDevice->Get().destroyDescriptorSetLayout(descriptorSetLayout);
        }

        mDevice->Get().destroyPipeline(mGraphicsPipeline);
        mDevice->Get().destroyPipelineLayout(mPipelineLayout);

        mDevice->Get().destroyRenderPass(mRenderPass);
    }

    /**
     * @brief Begins render pass, sets clear values, and binds graphics
     * pipeline.
     *
     * @param swapChain   Swapchain for framebuffer access
     * @param commandPool Command pool for current command buffer
     */
    void RenderPass::Begin(const Ref<SwapChain> swapChain,
                           const Ref<CommandPool>
                               commandPool) const

    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        auto clearValues = std::vector {
            vk::ClearValue().setColor(mFreyaOptions->clearColor),
            vk::ClearValue().setDepthStencil(
                vk::ClearDepthStencilValue().setDepth(1.0f)),
        };

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass)
                .setFramebuffer(swapChain->GetCurrentFrame().frameBuffer)
                .setRenderArea(vk::Rect2D().setOffset({ 0, 0 }).setExtent(
                    swapChain->GetExtent()))
                .setClearValues(clearValues),
            vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mGraphicsPipeline);

        BindDescriptorSet(commandPool, swapChain->GetCurrentFrameIndex());
    }

    /**
     * @brief Ends the render pass.
     * @param commandPool Command pool for current command buffer
     */
    void RenderPass::End(const Ref<CommandPool> commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        commandBuffer.endRenderPass();
    }

    /**
     * @brief Binds descriptor set at pipeline binding 0 for the given frame.
     *
     * @param commandPool Command pool for current command buffer
     * @param frameIndex  Frame index for descriptor set selection
     */
    void RenderPass::BindDescriptorSet(const Ref<CommandPool>& commandPool,
                                       const std::uint32_t     frameIndex) const
    {
        commandPool->GetCommandBuffer().bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mPipelineLayout,
            0,
            1,
            &mDescriptorSets[frameIndex],
            0,
            nullptr);
    }

    /**
     * @brief Updates projection uniform buffer for a given frame index.
     *
     * Copies data to uniform buffer and updates descriptor set with buffer
     * info.
     *
     * @param buffer     Projection data to upload
     * @param frameIndex Frame index for offset and descriptor set selection
     */
    void RenderPass::UpdateProjection(const ProjectionUniformBuffer& buffer,
                                      const std::uint32_t frameIndex) const
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