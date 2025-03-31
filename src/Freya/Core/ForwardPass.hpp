#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/RenderPass.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/UniformBuffer.hpp"

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
            mDevice(device), mSurface(surface), mUniformBuffer(uniformBuffer),
            mDescriptorSetLayouts(descriptorSetLayouts),
            mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool),
            mSamplerLayout(samplerLayout),
            mSamplerDescriptorPool(samplerDescriptorPool)
        {
        }

        ~ForwardPass();

        void UpdateProjection(const ProjectionUniformBuffer& buffer,
                              std::uint32_t                  frameIndex) const;

        void BindDescriptorSet(const Ref<CommandPool>& commandPool,
                               std::uint32_t           frameIndex) const;
        vk::PipelineLayout& GetPipelineLayout() { return mPipelineLayout; }
        vk::Pipeline&       GetGraphicsPipeline() { return mGraphicsPipeline; }

        vk::DescriptorSetLayout& GetSamplerLayout() { return mSamplerLayout; }
        vk::DescriptorPool&      GetSamplerDescriptorPool()
        {
            return mSamplerDescriptorPool;
        }

      private:
        Ref<Device>  mDevice;
        Ref<Surface> mSurface;

        Ref<Buffer>                          mUniformBuffer;
        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;
        vk::DescriptorSetLayout              mSamplerLayout;
        vk::DescriptorPool                   mSamplerDescriptorPool;
    };

} // namespace FREYA_NAMESPACE
