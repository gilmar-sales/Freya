#pragma once

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
     *
     * @param device         Device reference
     * @param physicalDevice Physical device reference
     * @param surface        Surface reference
     * @param freyaOptions   Freya options for configuration
     * @param logger         Logger reference
     * @param serviceProvider Service provider for shader module builder
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
            const skr::Arc<LightService>&                   lightService) :
            mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
            mFreyaOptions(freyaOptions), mLogger(logger),
            mServiceProvider(serviceProvider), mLightService(lightService)
        {
        }

        /**
         * @brief Builds and returns the RenderPass object.
         * @return Shared pointer to created RenderPass
         */
        skr::Arc<RenderPass> Build();

      private:
        /**
         * @brief Creates the Vulkan render pass with attachments and
         * dependencies.
         * @return Vulkan render pass handle
         */
        vk::RenderPass createRenderPass() const;

        /**
         * @brief Creates attachment descriptions for color, depth, and resolve.
         * @return Vector of attachment descriptions
         */
        std::vector<vk::AttachmentDescription> createAttachments() const;

        /**
         * @brief Creates subpass dependencies for forward or deferred
         * rendering.
         * @return Vector of subpass dependencies
         */
        std::vector<vk::SubpassDependency> createDependencies() const;

        skr::Arc<skr::Logger<RenderPassBuilder>> mLogger; ///< Logger reference
        skr::Arc<Device>                         mDevice; ///< Device reference
        skr::Arc<PhysicalDevice>       mPhysicalDevice;   ///< Physical device
        skr::Arc<Surface>              mSurface;          ///< Surface reference
        skr::Arc<skr::ServiceProvider> mServiceProvider;  ///< Service provider
        skr::Arc<FreyaOptions>         mFreyaOptions;     ///< Freya options
        skr::Arc<LightService>         mLightService;     ///< Light service
    };
} // namespace FREYA_NAMESPACE
