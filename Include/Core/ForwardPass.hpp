#pragma once

#include "Core/Buffer.hpp"
#include "Core/CommandPool.hpp"
#include "Core/Device.hpp"
#include "Core/Surface.hpp"
#include "Core/UniformBuffer.hpp"

namespace FREYA_NAMESPACE
{
    enum : std::uint32_t
    {
        ForwardColorAttachment,
        ForwardDepthAttachment,
        ForwardColorResolveAttachment
    };

    class ForwardPass
    {
      public:
        ForwardPass(
            const Ref<Device>&                          device,
            const Ref<Surface>&                         surface,
            const vk::RenderPass                        renderPass,
            const vk::PipelineLayout                    pipelineLayout,
            const vk::Pipeline                          graphicsPipeline,
            const Ref<Buffer>&                          uniformBuffer,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&       descriptorSets,
            const vk::DescriptorPool                    descriptorPool,
            const vk::DescriptorSetLayout               samplerLayout,
            const vk::DescriptorPool                    samplerDescriptorPool,
            const vk::Sampler                           sampler) :
            mDevice(device), mSurface(surface), mRenderPass(renderPass),
            mPipelineLayout(pipelineLayout),
            mGraphicsPipeline(graphicsPipeline), mUniformBuffer(uniformBuffer),
            mDescriptorSetLayouts(descriptorSetLayouts),
            mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool),
            mSamplerLayout(samplerLayout),
            mSamplerDescriptorPool(samplerDescriptorPool), mSampler(sampler),
            mIndex(0)
        {
        }

        ~ForwardPass();

        void UpdateProjection(const ProjectionUniformBuffer& buffer,
                              std::uint32_t            frameIndex) const;

        void BindDescriptorSet(const Ref<CommandPool>& commandPool,
                               std::uint32_t           frameIndex) const;

        vk::RenderPass&     Get() { return mRenderPass; }
        vk::PipelineLayout& GetPipelineLayout() { return mPipelineLayout; }
        vk::Pipeline&       GetGraphicsPipeline() { return mGraphicsPipeline; }

        vk::DescriptorSetLayout& GetSamplerLayout() { return mSamplerLayout; }
        vk::DescriptorPool&      GetSamplerDescriptorPool()
        {
            return mSamplerDescriptorPool;
        }
        vk::Sampler& GetSampler() { return mSampler; }

        vk::DescriptorSet& GetCurrentDescriptorSet()
        {
            return mDescriptorSets[mIndex];
        }

        void SetFrameIndex(const std::uint32_t index)
        {
            assert(index < mDescriptorSets.size() &&
                   "Cannot get vk::CommandBuffer: index out of bounds.");

            mIndex = index;
        }

      private:
        Ref<Device>  mDevice;
        Ref<Surface> mSurface;

        vk::RenderPass     mRenderPass;
        vk::PipelineLayout mPipelineLayout;
        vk::Pipeline       mGraphicsPipeline;

        Ref<Buffer>                          mUniformBuffer;
        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;
        vk::DescriptorSetLayout              mSamplerLayout;
        vk::DescriptorPool                   mSamplerDescriptorPool;
        vk::Sampler                          mSampler;
        std::uint32_t                        mIndex;
    };

} // namespace FREYA_NAMESPACE
