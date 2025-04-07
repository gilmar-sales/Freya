#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{

    enum : std::uint32_t
    {
        ColorAttachment,
        DepthAttachment,
        ColorResolveAttachment,
        GBufferAlbedoAttachment,
        GBufferPositionAttachment,
        GBufferNormalAttachment
    };

    class RenderPass
    {
      public:
        RenderPass(
            const Ref<Device>& device, const Ref<FreyaOptions>& freyaOptions,
            const vk::RenderPass                  renderPass,
            const vk::PipelineLayout              pipelineLayout,
            const vk::Pipeline                    graphicsPipeline,
            const vk::PipelineLayout              compositionPipelineLayout,
            const vk::Pipeline                    compositionPipeline,
            const std::vector<vk::DescriptorSet>& compositionDescriptorSets,
            const Ref<Buffer>&                    uniformBuffer,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&       descriptorSets,
            const vk::DescriptorPool                    descriptorPool,
            const vk::DescriptorSetLayout               samplerLayout,
            const vk::DescriptorPool                    samplerDescriptorPool) :
            mDevice(device), mFreyaOptions(freyaOptions),
            mRenderPass(renderPass), mPipelineLayout(pipelineLayout),
            mGraphicsPipeline(graphicsPipeline),
            mCompositionPipelineLayout(compositionPipelineLayout),
            mCompositionPipeline(compositionPipeline),
            mCompositionDescriptorSets(compositionDescriptorSets),
            mUniformBuffer(uniformBuffer),
            mDescriptorSetLayouts(descriptorSetLayouts),
            mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool),
            mSamplerLayout(samplerLayout),
            mSamplerDescriptorPool(samplerDescriptorPool)
        {
        }

        ~RenderPass();

        vk::RenderPass& Get() { return mRenderPass; }

        vk::PipelineLayout& GetPipelineLayout() { return mPipelineLayout; }

        void Begin(const Ref<SwapChain> swapChain,
                   const Ref<CommandPool>
                       commandPool) const;

        void End(const Ref<CommandPool> commandPool) const;

        void BindDescriptorSet(const Ref<CommandPool>& commandPool,
                               std::uint32_t           frameIndex) const;

        void UpdateProjection(const ProjectionUniformBuffer& buffer,
                              std::uint32_t                  frameIndex) const;

        vk::DescriptorSetLayout& GetSamplerLayout() { return mSamplerLayout; }

        vk::DescriptorPool& GetSamplerDescriptorPool()
        {
            return mSamplerDescriptorPool;
        }

        void SetOffscreenBuffers(Ref<SwapChain> swapChain);

        Ref<Device>       mDevice;
        Ref<FreyaOptions> mFreyaOptions;

        vk::RenderPass mRenderPass;

        vk::PipelineLayout mPipelineLayout;
        vk::Pipeline       mGraphicsPipeline;

        vk::PipelineLayout             mCompositionPipelineLayout;
        vk::Pipeline                   mCompositionPipeline;
        std::vector<vk::DescriptorSet> mCompositionDescriptorSets;

        Ref<Buffer> mUniformBuffer;

        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;

      private:
        vk::DescriptorSetLayout mSamplerLayout;
        vk::DescriptorPool      mSamplerDescriptorPool;
    };
} // namespace FREYA_NAMESPACE