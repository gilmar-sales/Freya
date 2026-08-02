#pragma once

#include "Freya/Core/IBLService.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/RenderPass.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class PhysicalDevice;
    class Device;
    class Surface;

    /**
     * @brief Builder for creating complete RenderPass with pipeline.
     *
     * Creates render pass, graphics pipeline with shaders from files,
     * descriptor sets/allocations, and uniform buffer. Supports MSAA
     * and forward/deferred rendering strategies.
     */
    class RenderPassBuilder
    {
      public:
        RenderPassBuilder(
            const skr::Arc<Device>&                         device,
            const skr::Arc<PhysicalDevice>&                 physicalDevice,
            const skr::Arc<Surface>&                        surface,
            const skr::Arc<FreyaOptions>&                   freyaOptions,
            const skr::Arc<skr::Logger<RenderPassBuilder>>& logger,
            const skr::Arc<skr::ServiceProvider>&           serviceProvider,
            const skr::Arc<LightService>&                   lightService,
            const skr::Arc<IBLService>&                     iblService) :
            mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
            mFreyaOptions(freyaOptions), mLogger(logger),
            mServiceProvider(serviceProvider), mLightService(lightService),
            mIblService(iblService)
        {
        }

        /**
         * @brief Builds and returns the RenderPass object.
         * @return Shared pointer to created RenderPass
         */
        skr::Arc<RenderPass> Build();

      private:
        vk::RenderPass createRenderPass() const;

        std::vector<vk::AttachmentDescription> createAttachments() const;

        std::vector<vk::SubpassDependency> createDependencies() const;

        skr::Arc<skr::Logger<RenderPassBuilder>> mLogger;
        skr::Arc<Device>                         mDevice;
        skr::Arc<PhysicalDevice>                 mPhysicalDevice;
        skr::Arc<Surface>                        mSurface;
        skr::Arc<skr::ServiceProvider>           mServiceProvider;
        skr::Arc<FreyaOptions>                   mFreyaOptions;
        skr::Arc<LightService>                   mLightService;
        skr::Arc<IBLService>                     mIblService;
    };
} // namespace FREYA_NAMESPACE
