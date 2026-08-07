#include "BloomPass.hpp"

namespace FREYA_NAMESPACE
{
    BloomPass::BloomPass(
        const skr::Arc<Device>&                     device,
        const skr::Arc<FreyaOptions>&               freyaOptions,
        const skr::Arc<Surface>&                    surface,
        vk::Extent2D                                halfExtent,
        vk::RenderPass                              renderPass,
        vk::PipelineLayout                          pipelineLayout,
        vk::Pipeline                                thresholdPipeline,
        vk::Pipeline                                downsamplePipeline,
        vk::Pipeline                                upsamplePipeline,
        const skr::Arc<Image>&                      bloomThresholdImage,
        const skr::Arc<Image>&                      bloomDownImage,
        const skr::Arc<Image>&                      bloomUpImage,
        const std::vector<vk::Framebuffer>&         framebuffers,
        vk::DescriptorPool                          descriptorPool,
        const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<vk::DescriptorSet>&       descriptorSets) :
        mDevice(device), mFreyaOptions(freyaOptions), mSurface(surface),
        mHalfExtent(halfExtent), mRenderPass(renderPass),
        mPipelineLayout(pipelineLayout),
        mBloomThresholdImage(bloomThresholdImage),
        mBloomDownImage(bloomDownImage), mBloomUpImage(bloomUpImage),
        mFramebuffers(framebuffers), mDescriptorPool(descriptorPool),
        mDescriptorSetLayouts(descriptorSetLayouts),
        mDescriptorSets(descriptorSets)
    {
        mPipelines[BloomThresholdSubpass]  = thresholdPipeline;
        mPipelines[BloomDownsampleSubpass] = downsamplePipeline;
        mPipelines[BloomUpsampleSubpass]   = upsamplePipeline;
    }

    BloomPass::~BloomPass()
    {
        auto& vkDevice = mDevice->Get();

        for (auto& fb : mFramebuffers)
            vkDevice.destroyFramebuffer(fb);

        vkDevice.destroyDescriptorPool(mDescriptorPool);

        // Destroy only unique layouts (all entries in the vector are the same)
        if (!mDescriptorSetLayouts.empty())
            vkDevice.destroyDescriptorSetLayout(mDescriptorSetLayouts[0]);

        for (auto& pipeline : mPipelines)
            vkDevice.destroyPipeline(pipeline);

        vkDevice.destroyPipelineLayout(mPipelineLayout);
        vkDevice.destroyRenderPass(mRenderPass);

        mBloomThresholdImage.reset();
        mBloomDownImage.reset();
        mBloomUpImage.reset();
    }

    vk::Pipeline& BloomPass::GetPipeline(const std::uint32_t subpass)
    {
        return mPipelines[subpass];
    }

    void BloomPass::Begin(const skr::Arc<SwapChain>    swapChain,
                          const skr::Arc<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, "Bloom Render Pass");

        auto clearValues = std::vector<vk::ClearValue> {
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // threshold
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // down
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // up
        };

        const auto imageIndex = swapChain->GetCurrentImageIndex();

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass)
                .setFramebuffer(mFramebuffers[imageIndex])
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(mHalfExtent))
                .setClearValues(clearValues),
            vk::SubpassContents::eInline);

        mLabelActive = false;
        BindPipeline(BloomThresholdSubpass, commandPool, 0);
    }

    void BloomPass::NextSubpass(const skr::Arc<CommandPool>& commandPool) const
    {
        commandPool->GetCommandBuffer().nextSubpass(
            vk::SubpassContents::eInline);
    }

    void BloomPass::BindPipeline(const std::uint32_t          subpass,
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
        mLabelActive = true;

        if (subpass == BloomThresholdSubpass ||
            subpass == BloomDownsampleSubpass ||
            subpass == BloomUpsampleSubpass)
        {
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mPipelineLayout,
                0,
                1,
                &mDescriptorSets[subpass],
                0,
                nullptr);
        }
    }

    void BloomPass::AdvanceSubpass(const std::uint32_t          subpass,
                                   const skr::Arc<CommandPool>& commandPool,
                                   const std::uint32_t frameIndex) const
    {
        NextSubpass(commandPool);

        BindPipeline(subpass, commandPool, frameIndex);
    }

    void BloomPass::DrawFullscreenTriangle(
        const skr::Arc<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        struct Push
        {
            float threshold;
            float extractScale;
        } push { mFreyaOptions->bloomThreshold,
                 mFreyaOptions->bloomExtractScale };
        commandBuffer.pushConstants(
            mPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(Push), &push);
        commandBuffer.draw(3, 1, 0, 0);
    }

    void BloomPass::End(const skr::Arc<CommandPool> commandPool) const
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

    const char* BloomPass::GetSubpassLabel(const std::uint32_t subpass)
    {
        switch (subpass)
        {
            case BloomThresholdSubpass:
                return "Bloom Threshold";
            case BloomDownsampleSubpass:
                return "Bloom Downsample";
            case BloomUpsampleSubpass:
                return "Bloom Upsample";
            default:
                return "Bloom Unknown";
        }
    }
} // namespace FREYA_NAMESPACE
