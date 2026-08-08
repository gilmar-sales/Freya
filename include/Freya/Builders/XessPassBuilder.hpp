#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/XessPass.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class XessPassBuilder
    {
      public:
        XessPassBuilder(const skr::Arc<Instance>&             instance,
                        const skr::Arc<PhysicalDevice>&       physicalDevice,
                        const skr::Arc<Device>&               device,
                        const skr::Arc<Surface>&              surface,
                        const skr::Arc<FreyaOptions>&         freyaOptions,
                        const skr::Arc<skr::ServiceProvider>& serviceProvider);

        /**
         * @brief Build XeSS pass targeting @p outputExtent (present size).
         *
         * Input resolution is queried from the SDK quality setting.
         * Returns nullptr when XeSS is unavailable or initialization fails.
         */
        skr::Arc<XessPass> Build(const skr::Arc<SwapChain>& swapChain,
                                 vk::Extent2D               outputExtent = {});

      private:
        skr::Arc<Instance>             mInstance;
        skr::Arc<PhysicalDevice>       mPhysicalDevice;
        skr::Arc<Device>               mDevice;
        skr::Arc<Surface>              mSurface;
        skr::Arc<FreyaOptions>         mFreyaOptions;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
    };
} // namespace FREYA_NAMESPACE
