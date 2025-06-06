#pragma once

#include "Device.hpp"
#include "Surface.hpp"

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

    class DeferredCompressedPass
    {
      public:
        explicit DeferredCompressedPass(const Ref<Device>&   device,
                                        const Ref<Surface>&  surface,
                                        const vk::RenderPass renderPass) :
            mDevice(device), mSurface(surface), mRenderPass(renderPass)
        {
        }

      private:
        Ref<Device>  mDevice;
        Ref<Surface> mSurface;

        vk::RenderPass mRenderPass;
    };

} // namespace FREYA_NAMESPACE