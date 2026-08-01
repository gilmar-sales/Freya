#pragma once

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class SwapChain;

    /**
     * @brief Builder for DeferredCompressedPass objects.
     *
     * Creates the full deferred rendering pipeline including:
     * - Vulkan render pass with 7 attachments and 5 subpasses
     * - 5 graphics pipelines (one per subpass)
     * - G-buffer, depth, translucent, and opaque images
     * - Framebuffers for each swapchain image
     * - Descriptor sets for UBO, input attachments, and samplers
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
            const skr::Arc<LightService>&         lightService) :
            mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
            mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider),
            mLightService(lightService)
        {
        }

        /**
         * @brief Builds and returns the DeferredCompressedPass object.
         * @param swapChain The current swapchain (used for framebuffer
         * creation)
         * @return Shared pointer to created DeferredCompressedPass
         */
        skr::Arc<DeferredCompressedPass> Build(const skr::Arc<SwapChain>& swapChain);

        /**
         * @brief Creates the Vulkan render pass for deferred rendering.
         * @return Vulkan render pass handle
         */
        vk::RenderPass createRenderPass() const;

      private:
        skr::Arc<Device>               mDevice;
        skr::Arc<PhysicalDevice>       mPhysicalDevice;
        skr::Arc<Surface>              mSurface;
        skr::Arc<FreyaOptions>         mFreyaOptions;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
        skr::Arc<LightService>         mLightService;
    };
} // namespace FREYA_NAMESPACE
