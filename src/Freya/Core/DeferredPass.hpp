#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/RenderPass.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    enum : std::uint32_t
    {
        DeferredPositionsAttachment,
        DeferredNormalsAttachment,
        DeferredAlbedoAttachment,
        DeferredDepthAttachment,
    };

    enum : std::uint32_t
    {
        DeferredGBufferPass,
        DeferredLightingPass,
    };

    struct DeferredPassAttachments
    {
        Ref<Image> Position;
        Ref<Image> Normal;
        Ref<Image> Albedo;
    };

    struct DeferredPassDescriptors
    {
        vk::DescriptorPool      pool;
        vk::DescriptorSetLayout layout;
        vk::DescriptorSet       composition;
    };

    struct DeferredPassPipeline
    {
        vk::PipelineLayout pipelineLayout;
        vk::Pipeline       pipeline;
    };

    class DeferredPass : public RenderPass
    {
      public:
        explicit DeferredPass(const Ref<Device>&       device,
                              const Ref<Surface>&      surface,
                              const vk::RenderPass     renderPass,
                              const vk::PipelineLayout pipelineLayout,
                              const vk::Pipeline       graphicsPipeline) :
            RenderPass(renderPass, pipelineLayout, graphicsPipeline),
            mDevice(device), mSurface(surface)
        {
        }

      private:
        Ref<Device>  mDevice;
        Ref<Surface> mSurface;
    };

} // namespace FREYA_NAMESPACE