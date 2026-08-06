#include "RenderPass.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Destroys all Vulkan resources: descriptor pools, pipeline, layout,
     * render pass.
     */
    RenderPass::~RenderPass()
    {
        mDevice->Get().destroySampler(mFallbackSampler);
        mDevice->Get().destroyImageView(mFallbackImageView);
        mDevice->Get().freeMemory(mFallbackImageMemory);
        mDevice->Get().destroyImage(mFallbackImage);

        mDevice->Get().destroySampler(mEmissiveFallbackSampler);
        mDevice->Get().destroyImageView(mEmissiveFallbackImageView);
        mDevice->Get().freeMemory(mEmissiveFallbackMemory);
        mDevice->Get().destroyImage(mEmissiveFallbackImage);

        mDevice->Get().destroyDescriptorPool(mSamplerDescriptorPool);

        mDevice->Get().destroyDescriptorPool(mDescriptorPool);

        mFallbackFactorsBuffer.reset();
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
    void RenderPass::Begin(const skr::Arc<SwapChain> swapChain,
                           const skr::Arc<CommandPool>
                               commandPool) const

    {
        const auto frameIndex = swapChain->GetCurrentFrameIndex();
        Begin(mRenderPass,
              swapChain->GetCurrentFrame().frameBuffer,
              swapChain->GetExtent(),
              frameIndex,
              commandPool);
    }

    /**
     * @brief Begins render pass with a custom render pass and framebuffer.
     *
     * Useful for offscreen forward rendering where the swapchain is not
     * the direct target. The render pass must be compatible (same
     * attachment formats, sample counts, subpass structure) so that
     * this forward pass's pipeline can be bound.
     *
     * @param renderPass  Compatible render pass handle
     * @param framebuffer Framebuffer handle (offscreen or swapchain)
     * @param extent      Render area extent
     * @param frameIndex  Frame index for descriptor set selection
     * @param commandPool Command pool for current command buffer
     */
    void RenderPass::Begin(const vk::RenderPass&        renderPass,
                           const vk::Framebuffer&       framebuffer,
                           const vk::Extent2D&          extent,
                           const std::uint32_t          frameIndex,
                           const skr::Arc<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        // Reverse-Z: clear depth to 0.0 (far plane) so that nearer fragments
        // pass the eGreaterOrEqual test.
        auto clearValues = std::vector {
            vk::ClearValue().setColor(mFreyaOptions->clearColor),
            vk::ClearValue().setDepthStencil(
                vk::ClearDepthStencilValue().setDepth(
                    mFreyaOptions->ReverseZ ? 0.0f : 1.0f)),
        };

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(renderPass)
                .setFramebuffer(framebuffer)
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(extent))
                .setClearValues(clearValues),
            vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mGraphicsPipeline);

        // Bind descriptor set 0 (UBO) and fallback set 1 (samplers) so that
        // draw calls without an explicit MaterialPool::Bind() still have valid
        // texture descriptors. MaterialPool::Bind() overwrites set 1 later.
        const auto descriptorSetsToBind = std::array {
            mDescriptorSets[frameIndex],
            mFallbackSamplerSet,
        };
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mPipelineLayout,
            0,
            descriptorSetsToBind,
            nullptr);
    }

    /**
     * @brief Ends the render pass.
     * @param commandPool Command pool for current command buffer
     */
    void RenderPass::End(const skr::Arc<CommandPool> commandPool) const
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
    void RenderPass::BindDescriptorSet(const skr::Arc<CommandPool>& commandPool,
                                       const std::uint32_t frameIndex) const
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
     * Copies data into the ring-buffer slot. Descriptor offsets are fixed
     * when the pass is created.
     *
     * @param buffer     Projection data to upload
     * @param frameIndex Frame index for offset and descriptor set selection
     */
    void RenderPass::UpdateProjection(const ProjectionUniformBuffer& buffer,
                                      const std::uint32_t frameIndex) const
    {
        const auto offset = frameIndex * sizeof(ProjectionUniformBuffer);
        mUniformBuffer->Copy(&buffer, sizeof(ProjectionUniformBuffer), offset);
    }

} // namespace FREYA_NAMESPACE
