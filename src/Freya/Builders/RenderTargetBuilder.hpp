#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/RenderTarget.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Fluent builder for offscreen RenderTarget objects.
     *
     * Prefer obtaining via Renderer::GetRenderTargetBuilder().
     */
    class RenderTargetBuilder
    {
      public:
        RenderTargetBuilder(
            const skr::Arc<Device>&               device,
            const skr::Arc<Surface>&              surface,
            const skr::Arc<skr::ServiceProvider>& serviceProvider);

        RenderTargetBuilder& SetWidth(const std::uint32_t width)
        {
            mWidth = width;
            return *this;
        }

        RenderTargetBuilder& SetHeight(const std::uint32_t height)
        {
            mHeight = height;
            return *this;
        }

        RenderTargetBuilder& SetFormat(const vk::Format format)
        {
            mFormat = format;
            return *this;
        }

        skr::Arc<RenderTarget> Build();

      private:
        vk::RenderPass createRenderPass(vk::Format format) const;

        skr::Arc<Device>               mDevice;
        skr::Arc<Surface>              mSurface;
        skr::Arc<skr::ServiceProvider> mServiceProvider;

        std::uint32_t mWidth  = 1280;
        std::uint32_t mHeight = 720;
        vk::Format    mFormat = vk::Format::eUndefined;
    };

} // namespace FREYA_NAMESPACE
