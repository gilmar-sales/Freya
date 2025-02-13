#pragma once
#include "Device.hpp"

namespace FREYA_NAMESPACE
{
    enum : std::uint32_t
    {
        DeferredBackAttachment,
        DeferredDepthAttachment,
        DeferredGBufferAttachment,
        DeferredTranslucentAttachment,
        DeferredOpaqueAttachment
    };

    enum : std::uint32_t
    {
        DeferredDepthPrePass,
        DeferredGBufferPass,
        DeferredLightingPass,
        DeferredTranslucentPass,
        DeferredCompositePass
    };

    class DeferredPass
    {
      public:
        explicit DeferredPass(const Ref<Device>& device, const Ref<Surface>& surface, const vk::RenderPass renderPass) :
            mDevice(device),
            mSurface(surface),
            mRenderPass(renderPass) {}

      private:
        Ref<Device>  mDevice;
        Ref<Surface> mSurface;

        vk::RenderPass mRenderPass;
    };

} // namespace FREYA_NAMESPACE