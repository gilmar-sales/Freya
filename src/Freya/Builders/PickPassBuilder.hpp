#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/PickPass.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Builder for PickPass objects.
     *
     * Creates the R32_UINT color + depth render pass, pick graphics pipeline
     * (projection UBO set 0 + entity push constant), descriptor set, staging
     * readback buffer, and an initial framebuffer.
     */
    class PickPassBuilder
    {
      public:
        PickPassBuilder(
            const skr::Arc<Device>&               device,
            const skr::Arc<PhysicalDevice>&       physicalDevice,
            const skr::Arc<FreyaOptions>&         freyaOptions,
            const skr::Arc<skr::ServiceProvider>& serviceProvider) :
            mDevice(device), mPhysicalDevice(physicalDevice),
            mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider)
        {
        }

        /**
         * @brief Builds and returns the PickPass object.
         */
        skr::Arc<PickPass> Build(vk::Extent2D extent = { 1, 1 });

      private:
        vk::RenderPass createRenderPass(vk::Format depthFormat) const;

        skr::Arc<Device>               mDevice;
        skr::Arc<PhysicalDevice>       mPhysicalDevice;
        skr::Arc<FreyaOptions>         mFreyaOptions;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
    };

} // namespace FREYA_NAMESPACE
