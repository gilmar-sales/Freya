#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/FsrUpscalePass.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class FsrUpscalePassBuilder
    {
      public:
        FsrUpscalePassBuilder(
            const skr::Arc<Device>&               device,
            const skr::Arc<PhysicalDevice>&       physicalDevice,
            const skr::Arc<Surface>&              surface,
            const skr::Arc<FreyaOptions>&         freyaOptions,
            const skr::Arc<skr::ServiceProvider>& serviceProvider);

        skr::Arc<FsrUpscalePass> Build(const skr::Arc<SwapChain>& swapChain,
                                       vk::Extent2D renderExtent  = {},
                                       vk::Extent2D displayExtent = {});

      private:
        skr::Arc<Device>               mDevice;
        skr::Arc<PhysicalDevice>       mPhysicalDevice;
        skr::Arc<Surface>              mSurface;
        skr::Arc<FreyaOptions>         mFreyaOptions;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
    };
} // namespace FREYA_NAMESPACE
