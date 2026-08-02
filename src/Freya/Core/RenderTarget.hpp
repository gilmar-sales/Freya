#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"

#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Offscreen color target for scene output (e.g. ImGui viewport).
     *
     * Owns a color image (ColorAttachment | Sampled), a compatible composite
     * render pass with ShaderReadOnlyOptimal final layout, a framebuffer, and
     * a linear sampler for UI sampling.
     */
    class RenderTarget
    {
      public:
        RenderTarget(const skr::Arc<Device>& device,
                     const skr::Arc<Image>&  colorImage,
                     vk::Extent2D            extent,
                     vk::Format              format,
                     vk::RenderPass          renderPass,
                     vk::Framebuffer         framebuffer,
                     vk::Sampler             sampler);

        ~RenderTarget();

        [[nodiscard]] vk::Extent2D GetExtent() const { return mExtent; }

        [[nodiscard]] vk::Format GetFormat() const { return mFormat; }

        [[nodiscard]] skr::Arc<Image> GetColorImage() const
        {
            return mColorImage;
        }

        [[nodiscard]] vk::ImageView& GetColorImageView()
        {
            return mColorImage->GetImageView();
        }

        [[nodiscard]] vk::Sampler GetSampler() const { return mSampler; }

        [[nodiscard]] vk::RenderPass GetRenderPass() const
        {
            return mRenderPass;
        }

        [[nodiscard]] vk::Framebuffer GetFramebuffer() const
        {
            return mFramebuffer;
        }

      private:
        skr::Arc<Device> mDevice;
        skr::Arc<Image>  mColorImage;
        vk::Extent2D     mExtent;
        vk::Format       mFormat;
        vk::RenderPass   mRenderPass;
        vk::Framebuffer  mFramebuffer;
        vk::Sampler      mSampler;
    };

} // namespace FREYA_NAMESPACE
