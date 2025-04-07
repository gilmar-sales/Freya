#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/RenderPass.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    enum : std::uint32_t
    {
        ForwardColorAttachment,
        ForwardDepthAttachment,
        ForwardColorResolveAttachment
    };

    class ForwardPass : public RenderPass
    {
      public:
        ForwardPass(
            const Ref<Device>&                          device,
            const Ref<Surface>&                         surface,
            const Ref<FreyaOptions>&                    freyaOptions,
            const vk::RenderPass                        renderPass,
            const vk::PipelineLayout                    pipelineLayout,
            const vk::Pipeline                          graphicsPipeline,
            const Ref<Buffer>&                          uniformBuffer,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&       descriptorSets,
            const vk::DescriptorPool                    descriptorPool,
            const vk::DescriptorSetLayout               samplerLayout,
            const vk::DescriptorPool                    samplerDescriptorPool) :
            RenderPass(renderPass, pipelineLayout, graphicsPipeline),
            mDevice(device), mSurface(surface), mFreyaOptions(freyaOptions),
            mUniformBuffer(uniformBuffer),
            mDescriptorSetLayouts(descriptorSetLayouts),
            mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool),
            mSamplerLayout(samplerLayout),
            mSamplerDescriptorPool(samplerDescriptorPool)
        {
        }

        ~ForwardPass();

        void Begin(const Ref<SwapChain> swapChain,
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
                                       GetGraphicsPipeline());

            BindDescriptorSet(commandPool, mCurrentFrameIndex);
        }

        void End(const Ref<CommandPool> commandPool) const
        {
            auto commandBuffer = commandPool->GetCommandBuffer();

            commandBuffer.endRenderPass();
        }

        void UpdateProjection(const ProjectionUniformBuffer& buffer,
                              std::uint32_t                  frameIndex) const;

        void BindDescriptorSet(const Ref<CommandPool>& commandPool,
                               std::uint32_t           frameIndex) const;

        vk::PipelineLayout& GetPipelineLayout() { return mPipelineLayout; }

        const vk::Pipeline& GetGraphicsPipeline() const
        {
            return mGraphicsPipeline;
        }

        vk::DescriptorSetLayout& GetSamplerLayout() { return mSamplerLayout; }
        vk::DescriptorPool&      GetSamplerDescriptorPool()
        {
            return mSamplerDescriptorPool;
        }

      private:
        Ref<Device>       mDevice;
        Ref<Surface>      mSurface;
        Ref<FreyaOptions> mFreyaOptions;

        Ref<Buffer>   mUniformBuffer;
        std::uint32_t mCurrentFrameIndex = 0;

        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;
        vk::DescriptorSetLayout              mSamplerLayout;
        vk::DescriptorPool                   mSamplerDescriptorPool;
    };

} // namespace FREYA_NAMESPACE
