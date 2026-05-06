#include "BloomPass.hpp"

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
    BloomPass::BloomPass(
        const Ref<Device>&                          device,
        const Ref<FreyaOptions>&                    freyaOptions,
        const Ref<Surface>&                         surface,
        vk::Extent2D                                halfExtent,
        vk::RenderPass                              renderPass,
        vk::PipelineLayout                          pipelineLayout,
        vk::Pipeline                                thresholdPipeline,
        vk::Pipeline                                downsamplePipeline,
        vk::Pipeline                                upsamplePipeline,
        const Ref<Image>&                           bloomThresholdImage,
        const Ref<Image>&                           bloomDownImage,
        const Ref<Image>&                           bloomUpImage,
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

    void BloomPass::Begin(const Ref<SwapChain>    swapChain,
                          const Ref<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        beginDebugLabel(commandBuffer, "Bloom Render Pass", mDevice->Get());

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

    void BloomPass::NextSubpass(const Ref<CommandPool>& commandPool) const
    {
        commandPool->GetCommandBuffer().nextSubpass(
            vk::SubpassContents::eInline);
    }

    void BloomPass::BindPipeline(const std::uint32_t     subpass,
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

    void BloomPass::AdvanceSubpass(const std::uint32_t     subpass,
                                   const Ref<CommandPool>& commandPool,
                                   const std::uint32_t     frameIndex) const
    {
        NextSubpass(commandPool);

        BindPipeline(subpass, commandPool, frameIndex);
    }

    void BloomPass::DrawFullscreenTriangle(
        const Ref<CommandPool>& commandPool) const
    {
        commandPool->GetCommandBuffer().draw(3, 1, 0, 0);
    }

    void BloomPass::End(const Ref<CommandPool> commandPool) const
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
