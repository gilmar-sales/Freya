#include "RenderPass.hpp"

namespace FREYA_NAMESPACE
{
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

        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            clearValues.push_back(
                vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }));

            clearValues.push_back(
                vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }));

            clearValues.push_back(
                vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }));
        }

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

    void RenderPass::End(const Ref<CommandPool> commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            commandBuffer.nextSubpass(vk::SubpassContents::eInline);

            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                       mCompositionPipeline);

            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, mCompositionPipelineLayout, 0,
                1, &mCompositionDescriptorSets[0], 0, nullptr);

            commandBuffer.draw(3, 1, 0, 0);
        }

        commandBuffer.endRenderPass();
    }
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

    void RenderPass::SetOffscreenBuffers(Ref<SwapChain> swapChain)
    {
        for (auto& offscreenBuffer : swapChain->GetOffscreenBuffers())
        {
            auto albedoImageInfo =
                vk::DescriptorImageInfo()
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setImageView(offscreenBuffer.albedo->GetImageView());

            auto normalImageInfo =
                vk::DescriptorImageInfo()
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setImageView(offscreenBuffer.normal->GetImageView())
                    .setSampler(nullptr);

            auto positionImageInfo =
                vk::DescriptorImageInfo()
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setImageView(offscreenBuffer.position->GetImageView());

            auto writeDescriptorSets = std::vector {
                vk::WriteDescriptorSet()
                    .setDescriptorType(vk::DescriptorType::eInputAttachment)
                    .setImageInfo(albedoImageInfo)
                    .setDstBinding(0)
                    .setDstSet(mCompositionDescriptorSets[0]),
                vk::WriteDescriptorSet()
                    .setDescriptorType(vk::DescriptorType::eInputAttachment)
                    .setImageInfo(normalImageInfo)
                    .setDstBinding(1)
                    .setDstSet(mCompositionDescriptorSets[0]),
                vk::WriteDescriptorSet()
                    .setDescriptorType(vk::DescriptorType::eInputAttachment)
                    .setImageInfo(positionImageInfo)
                    .setDstBinding(2)
                    .setDstSet(mCompositionDescriptorSets[0]),
            };

            mDevice->Get().updateDescriptorSets(
                writeDescriptorSets.size(), writeDescriptorSets.data(), 0,
                nullptr);
        }
    }

} // namespace FREYA_NAMESPACE