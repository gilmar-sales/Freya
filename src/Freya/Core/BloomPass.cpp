#include "BloomPass.hpp"

namespace FREYA_NAMESPACE
{
    BloomPass::BloomPass(
        const skr::Arc<Device>&       device,
        const skr::Arc<FreyaOptions>& freyaOptions,
        const skr::Arc<Surface>&      surface,
        vk::Extent2D                  halfExtent,
        vk::RenderPass                renderPass,
        vk::PipelineLayout            pipelineLayout,
        vk::Pipeline                  thresholdPipeline,
        vk::Pipeline                  downsamplePipeline,
        vk::Pipeline                  upsamplePipeline,
        std::vector<skr::Arc<Image>>
            bloomThresholdImages,
        std::vector<skr::Arc<Image>>
            bloomDownImages,
        std::vector<skr::Arc<Image>>
                                                    bloomUpImages,
        const std::vector<vk::Framebuffer>&         framebuffers,
        vk::DescriptorPool                          descriptorPool,
        const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<vk::DescriptorSet>&       descriptorSets,
        const vk::Sampler                           sampler) :
        mDevice(device), mFreyaOptions(freyaOptions), mSurface(surface),
        mHalfExtent(halfExtent), mRenderPass(renderPass),
        mPipelineLayout(pipelineLayout),
        mBloomThresholdImages(std::move(bloomThresholdImages)),
        mBloomDownImages(std::move(bloomDownImages)),
        mBloomUpImages(std::move(bloomUpImages)), mFramebuffers(framebuffers),
        mDescriptorPool(descriptorPool),
        mDescriptorSetLayouts(descriptorSetLayouts),
        mDescriptorSets(descriptorSets), mSampler(sampler)
    {
        mPipelines[BloomThresholdSubpass]  = thresholdPipeline;
        mPipelines[BloomDownsampleSubpass] = downsamplePipeline;
        mPipelines[BloomUpsampleSubpass]   = upsamplePipeline;
    }

    BloomPass::~BloomPass()
    {
        mDevice->Get().waitIdle();
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
        if (mSampler)
            vkDevice.destroySampler(mSampler);

        mBloomThresholdImages.clear();
        mBloomDownImages.clear();
        mBloomUpImages.clear();
    }

    skr::Arc<Image> BloomPass::GetBloomUpImage(
        const std::uint32_t frameIndex) const
    {
        if (frameIndex >= mBloomUpImages.size())
            return {};
        return mBloomUpImages[frameIndex];
    }

    void BloomPass::SetThresholdInput(const std::uint32_t    frameIndex,
                                      const skr::Arc<Image>& sourceImage)
    {
        if (!sourceImage || !mSampler)
            return;
        const auto setIndex = frameIndex * 3u + BloomThresholdSubpass;
        if (setIndex >= mDescriptorSets.size())
            return;

        auto info = vk::DescriptorImageInfo()
                        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setImageView(sourceImage->GetImageView())
                        .setSampler(mSampler);
        auto writer =
            vk::WriteDescriptorSet()
                .setDstSet(mDescriptorSets[setIndex])
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(info);
        mDevice->Get().updateDescriptorSets(1, &writer, 0, nullptr);
    }

    vk::Pipeline& BloomPass::GetPipeline(const std::uint32_t subpass)
    {
        return mPipelines[subpass];
    }

    void BloomPass::Begin(const skr::Arc<CommandPool>& commandPool,
                          const std::uint32_t          frameIndex) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::Bloom);

        auto clearValues = std::vector<vk::ClearValue> {
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // threshold
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // down
            vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }), // up
        };

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass)
                .setFramebuffer(mFramebuffers[frameIndex])
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(mHalfExtent))
                .setClearValues(clearValues),
            vk::SubpassContents::eInline);

        mLabelActive = false;
        BindPipeline(BloomThresholdSubpass, commandPool, frameIndex);
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

        mDevice->BeginDebugLabel(commandBuffer, GetSubpassRegion(subpass));
        mLabelActive = true;

        if (subpass == BloomThresholdSubpass ||
            subpass == BloomDownsampleSubpass ||
            subpass == BloomUpsampleSubpass)
        {
            const auto setIndex = frameIndex * 3u + subpass;
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mPipelineLayout,
                0,
                1,
                &mDescriptorSets[setIndex],
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
        return GetSubpassRegion(subpass).name;
    }

    DebugRegion BloomPass::GetSubpassRegion(const std::uint32_t subpass)
    {
        switch (subpass)
        {
            case BloomThresholdSubpass:
                return DebugLabel::BloomThreshold;
            case BloomDownsampleSubpass:
                return DebugLabel::BloomDownsample;
            case BloomUpsampleSubpass:
                return DebugLabel::BloomUpsample;
            default:
                return { "Bloom Unknown", DebugLabel::BloomColor };
        }
    }
} // namespace FREYA_NAMESPACE
