#pragma once

#include "Device.hpp"

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

    class DeferredPass
    {
      public:
        explicit DeferredPass(const Ref<Device>&   device,
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