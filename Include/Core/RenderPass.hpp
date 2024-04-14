#pragma once

#include "Core/Buffer.hpp"
#include "Core/CommandPool.hpp"
#include "Core/Device.hpp"
#include "Core/Surface.hpp"
#include "Core/UniformBuffer.hpp"

namespace FREYA_NAMESPACE
{

    class RenderPass
    {
      public:
        RenderPass(std::shared_ptr<Device> device,
                   std::shared_ptr<Surface>
                                      surface,
                   vk::RenderPass     renderPass,
                   vk::PipelineLayout pipelineLayout,
                   vk::Pipeline       graphicsPipeline,
                   std::vector<std::shared_ptr<Buffer>>
                       uniformBuffers,
                   std::vector<vk::DescriptorSetLayout>
                       descriptorSetLayouts,
                   std::vector<vk::DescriptorSet>
                                      descriptorSets,
                   vk::DescriptorPool descriptorPool) :
            mDevice(device),
            mSurface(surface), mRenderPass(renderPass),
            mPipelineLayout(pipelineLayout), mGraphicsPipeline(graphicsPipeline),
            mUniformBuffers(uniformBuffers),
            mDescriptorSetLayouts(descriptorSetLayouts),
            mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool)
        {
        }

        ~RenderPass();

        void UpdateProjection(ProjectionUniformBuffer& buffer, std::uint32_t frameIndex);
        void UpdateModel(glm::mat4 model, std::uint32_t frameIndex);

        void BindDescriptorSet(std::shared_ptr<CommandPool> commandPool,
                               std::uint32_t                frameIndex);

        vk::RenderPass&     Get() { return mRenderPass; }
        vk::PipelineLayout& GetPipelineLayout() { return mPipelineLayout; }
        vk::Pipeline&       GetGraphicsPipeline() { return mGraphicsPipeline; }

      private:
        std::shared_ptr<Device>  mDevice;
        std::shared_ptr<Surface> mSurface;

        vk::RenderPass     mRenderPass;
        vk::PipelineLayout mPipelineLayout;
        vk::Pipeline       mGraphicsPipeline;

        std::vector<std::shared_ptr<Buffer>> mUniformBuffers;
        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;
    };

} // namespace FREYA_NAMESPACE
