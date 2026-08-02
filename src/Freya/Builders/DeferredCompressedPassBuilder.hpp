#pragma once

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/IBLService.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class SwapChain;

    /**
     * @brief Builder for DeferredCompressedPass objects.
     */
    class DeferredCompressedPassBuilder
    {
      public:
        DeferredCompressedPassBuilder(
            const skr::Arc<Device>&               device,
            const skr::Arc<PhysicalDevice>&       physicalDevice,
            const skr::Arc<Surface>&              surface,
            const skr::Arc<FreyaOptions>&         freyaOptions,
            const skr::Arc<skr::ServiceProvider>& serviceProvider,
            const skr::Arc<LightService>&         lightService,
            const skr::Arc<IBLService>&           iblService) :
            mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
            mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider),
            mLightService(lightService), mIblService(iblService)
        {
        }

        skr::Arc<DeferredCompressedPass> Build(
            const skr::Arc<SwapChain>& swapChain, vk::Extent2D extent = {});

        vk::RenderPass createRenderPass() const;

      private:
        skr::Arc<Device>               mDevice;
        skr::Arc<PhysicalDevice>       mPhysicalDevice;
        skr::Arc<Surface>              mSurface;
        skr::Arc<FreyaOptions>         mFreyaOptions;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
        skr::Arc<LightService>         mLightService;
        skr::Arc<IBLService>           mIblService;
    };
} // namespace FREYA_NAMESPACE
