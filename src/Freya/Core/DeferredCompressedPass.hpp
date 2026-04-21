#pragma once

#include "Device.hpp"
#include "Surface.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Attachment indices for deferred rendering pass.
     */
    enum : std::uint32_t
    {
        DeferredBackAttachment,        ///< Back buffer color attachment
        DeferredDepthAttachment,       ///< Depth attachment
        DeferredGBufferAttachment,     ///< G-buffer position/normal
        DeferredTranslucentAttachment, ///< Translucent objects buffer
        DeferredOpaqueAttachment       ///< Opaque objects buffer
    };

    /**
     * @brief Subpass indices for deferred rendering pipeline.
     */
    enum : std::uint32_t
    {
        DeferredDepthPrePass,    ///< Depth pre-pass
        DeferredGBufferPass,     ///< G-buffer generation
        DeferredLightingPass,    ///< Lighting calculation
        DeferredTranslucentPass, ///< Translucent object rendering
        DeferredCompositePass    ///< Final compositing
    };

    /**
     * @brief Deferred rendering pass with G-buffer attachments.
     *
     * Manages a deferred render pass with multiple attachments:
     * - Back buffer (color)
     * - Depth attachment
     * - G-buffer (position, normal, albedo)
     * - Translucent and opaque buffers
     *
     * Uses subpasses for depth pre-pass, G-buffer, lighting, and compositing.
     *
     * @param device    Device reference
     * @param surface   Surface reference
     * @param renderPass Vulkan render pass handle
     */
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