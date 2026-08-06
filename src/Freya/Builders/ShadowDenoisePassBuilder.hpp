#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/ShadowDenoisePass.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class ShadowDenoisePassBuilder
    {
      public:
        ShadowDenoisePassBuilder(
            const skr::Arc<Device>&               device,
            const skr::Arc<PhysicalDevice>&       physicalDevice,
            const skr::Arc<Surface>&              surface,
            const skr::Arc<FreyaOptions>&         freyaOptions,
            const skr::Arc<skr::ServiceProvider>& serviceProvider,
            const skr::Arc<ShadowPass>&           shadowPass);

        skr::Arc<ShadowDenoisePass> Build(
            const skr::Arc<SwapChain>& swapChain,
            const skr::Arc<Image>&     depthImage,
            const skr::Arc<Image>&     normalImage,
            vk::Extent2D               fullExtent = {});

      private:
        vk::RenderPass createColorPass(vk::Format format) const;

        skr::Arc<Device>               mDevice;
        skr::Arc<PhysicalDevice>       mPhysicalDevice;
        skr::Arc<Surface>              mSurface;
        skr::Arc<FreyaOptions>         mFreyaOptions;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
        skr::Arc<ShadowPass>           mShadowPass;
    };
} // namespace FREYA_NAMESPACE
