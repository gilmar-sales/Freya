#pragma once

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
        RenderPassBuilder(const Ref<Device>&         device,
                          const Ref<PhysicalDevice>& physicalDevice,
                          const Ref<Surface>&        surface,
                          const Ref<FreyaOptions>&   freyaOptions,
                          const Ref<skr::Logger<RenderPassBuilder>>& logger,
                          const Ref<skr::ServiceProvider>& serviceProvider) :
            mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
            mFreyaOptions(freyaOptions), mLogger(logger),
            mServiceProvider(serviceProvider)
        {
        }

        /**
         * @brief Builds and returns the RenderPass object.
         * @return Shared pointer to created RenderPass
         */
        Ref<RenderPass> Build();

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

        Ref<skr::Logger<RenderPassBuilder>> mLogger; ///< Logger reference
        Ref<Device>                         mDevice; ///< Device reference
        Ref<PhysicalDevice> mPhysicalDevice; ///< Physical device reference
        Ref<Surface>        mSurface;        ///< Surface reference
        Ref<skr::ServiceProvider>
                          mServiceProvider; ///< Service provider reference
        Ref<FreyaOptions> mFreyaOptions;    ///< Freya options reference
    };
} // namespace FREYA_NAMESPACE
