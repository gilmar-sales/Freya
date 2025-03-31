#pragma once

namespace FREYA_NAMESPACE
{
    class RenderPass
    {
      public:
        vk::RenderPass& Get() { return mRenderPass; }

      protected:
        RenderPass(const vk::RenderPass     renderPass,
                   const vk::PipelineLayout pipelineLayout,
                   const vk::Pipeline       graphicsPipeline) :
            mRenderPass(renderPass), mPipelineLayout(pipelineLayout),
            mGraphicsPipeline(graphicsPipeline)
        {
        }

        vk::RenderPass     mRenderPass;
        vk::PipelineLayout mPipelineLayout;
        vk::Pipeline       mGraphicsPipeline;
    };
} // namespace FREYA_NAMESPACE