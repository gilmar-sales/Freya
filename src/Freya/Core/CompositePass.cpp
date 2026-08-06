#include "CompositePass.hpp"

#include <vulkan/vulkan.h>

namespace FREYA_NAMESPACE
{
    CompositePass::CompositePass(
        const skr::Arc<Device>&               device,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<Surface>&              surface,
        vk::RenderPass                        renderPass,
        vk::PipelineLayout                    pipelineLayout,
        vk::Pipeline                          compositePipeline,
        const std::vector<vk::Framebuffer>&   framebuffers,
        vk::DescriptorPool                    descriptorPool,
        const vk::DescriptorSetLayout         descriptorSetLayout,
        const std::vector<vk::DescriptorSet>& descriptorSets) :
        mDevice(device), mFreyaOptions(freyaOptions), mSurface(surface),
        mRenderPass(renderPass), mPipelineLayout(pipelineLayout),
        mCompositePipeline(compositePipeline), mFramebuffers(framebuffers),
        mDescriptorPool(descriptorPool),
        mDescriptorSetLayout(descriptorSetLayout),
        mDescriptorSets(descriptorSets), mBoundImages(descriptorSets.size())
    {
    }

    CompositePass::~CompositePass()
    {
        auto& vkDevice = mDevice->Get();

        for (auto& fb : mFramebuffers)
            vkDevice.destroyFramebuffer(fb);

        vkDevice.destroyPipeline(mCompositePipeline);
        vkDevice.destroyPipelineLayout(mPipelineLayout);
        vkDevice.destroyDescriptorPool(mDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mDescriptorSetLayout);
        vkDevice.destroyRenderPass(mRenderPass);
    }

    void CompositePass::Begin(const skr::Arc<SwapChain>    swapChain,
                              const skr::Arc<CommandPool>& commandPool,
                              const vk::ClearValue&        clearColor) const
    {
        auto       commandBuffer = commandPool->GetCommandBuffer();
        const auto imageIndex    = swapChain->GetCurrentImageIndex();

        auto clearValues = std::vector<vk::ClearValue> { clearColor };

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass)
                .setFramebuffer(mFramebuffers[imageIndex])
                .setRenderArea(vk::Rect2D().setOffset({ 0, 0 }).setExtent(
                    swapChain->GetExtent()))
                .setClearValues(clearValues),
            vk::SubpassContents::eInline);
    }

    void CompositePass::Begin(const vk::RenderPass         renderPass,
                              const vk::Framebuffer        framebuffer,
                              const vk::Extent2D           extent,
                              const skr::Arc<CommandPool>& commandPool,
                              const vk::ClearValue&        clearColor) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        auto clearValues   = std::vector<vk::ClearValue> { clearColor };

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(renderPass)
                .setFramebuffer(framebuffer)
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(extent))
                .setClearValues(clearValues),
            vk::SubpassContents::eInline);
    }

    void CompositePass::BindPipeline(const skr::Arc<CommandPool>& commandPool,
                                     const std::uint32_t frameIndex) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mCompositePipeline);

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mPipelineLayout,
            0,
            1,
            &mDescriptorSets[frameIndex],
            0,
            nullptr);
    }

    void CompositePass::DrawFullscreenTriangle(
        const skr::Arc<CommandPool>& commandPool, const float tonemapHdr) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        commandBuffer.pushConstants(
            mPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(float), &tonemapHdr);
        commandBuffer.draw(3, 1, 0, 0);
    }

    void CompositePass::End(const skr::Arc<CommandPool> commandPool) const
    {
        commandPool->GetCommandBuffer().endRenderPass();
    }

    void CompositePass::UpdateDescriptorSet(
        const std::uint32_t frameIndex, const skr::Arc<Image>& opaqueImage,
        const skr::Arc<Image>& translucentImage,
        const skr::Arc<Image>& bloomResultImage, vk::Sampler sampler)
    {
        if (frameIndex >= mBoundImages.size())
            return;

        const auto opaqueView = opaqueImage->GetImageView();
        const auto translView = translucentImage->GetImageView();
        const auto bloomView  = bloomResultImage->GetImageView();

        auto& bound = mBoundImages[frameIndex];
        if (bound.opaque == opaqueView && bound.translucent == translView &&
            bound.bloom == bloomView && bound.sampler == sampler)
        {
            return;
        }

        bound.opaque      = opaqueView;
        bound.translucent = translView;
        bound.bloom       = bloomView;
        bound.sampler     = sampler;

        auto opaqueInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(opaqueView)
                .setSampler(sampler);

        auto translInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(translView)
                .setSampler(sampler);

        auto bloomInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(bloomView)
                .setSampler(sampler);

        auto writes = std::array {
            vk::WriteDescriptorSet()
                .setDstSet(mDescriptorSets[frameIndex])
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(opaqueInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mDescriptorSets[frameIndex])
                .setDstBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(translInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mDescriptorSets[frameIndex])
                .setDstBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(bloomInfo),
        };

        mDevice->Get().updateDescriptorSets(
            static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
} // namespace FREYA_NAMESPACE
