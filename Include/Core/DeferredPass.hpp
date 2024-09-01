#pragma once
#include "Device.hpp"

namespace FREYA_NAMESPACE
{
    enum : std::uint32_t
    {
        BackAttachment,
        DepthAttachment,
        GBufferAttachment,
        TranslucentAttachment,
        OpaqueAttachment
    };

    enum : std::uint32_t
    {
        DepthPrePass,
        GBufferPass,
        LightingPass,
        TranslucentPass,
        CompositePass
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