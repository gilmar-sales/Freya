#pragma once

#include "Freya/Core/SwapChain.hpp"

#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class Instance;
    class PhysicalDevice;
    class Device;
    class Surface;
    class RenderPass;

    /**
     * @brief Builder for creating SwapChain objects.
     *
     * Queries surface capabilities, creates swapchain with appropriate
     * image count, format, and present mode. Creates depth and sample
     * images, framebuffers, and synchronization objects.
     *
     * @param instance      Instance reference
     * @param physicalDevice Physical device reference
     * @param device        Device reference
     * @param surface        Surface reference
     * @param renderPass    Render pass reference
     * @param freyaOptions   Freya options reference
     * @param logger         Logger reference
     * @param serviceProvider Service provider for image builder
     */
    class SwapChainBuilder
    {
      public:
        SwapChainBuilder(
            const skr::Arc<Instance>&                      instance,
            const skr::Arc<PhysicalDevice>&                physicalDevice,
            const skr::Arc<Device>&                        device,
            const skr::Arc<Surface>&                       surface,
            const skr::Arc<RenderPass>&                    renderPass,
            const skr::Arc<FreyaOptions>&                  freyaOptions,
            const skr::Arc<skr::Logger<SwapChainBuilder>>& logger,
            const skr::Arc<skr::ServiceProvider>&          serviceProvider) :
            mInstance(instance), mPhysicalDevice(physicalDevice),
            mDevice(device), mSurface(surface), mRenderPass(renderPass),
            mFreyaOptions(freyaOptions), mLogger(logger),
            mServiceProvider(serviceProvider)
        {
        }

        /**
         * @brief Builds and returns the SwapChain object.
         * @return Shared pointer to created SwapChain
         */
        skr::Arc<SwapChain> Build();

      protected:
        /**
         * @brief Chooses the best present mode based on VSync setting.
         * @return Selected present mode
         */
        vk::PresentModeKHR choosePresentMode();

      private:
        skr::Arc<Instance>       mInstance;       ///< Instance reference
        skr::Arc<PhysicalDevice> mPhysicalDevice; ///< Physical device reference
        skr::Arc<Device>         mDevice;         ///< Device reference
        skr::Arc<Surface>        mSurface;        ///< Surface reference
        skr::Arc<RenderPass>     mRenderPass;     ///< Render pass reference
        skr::Arc<FreyaOptions>   mFreyaOptions;   ///< Freya options reference

        skr::Arc<skr::Logger<SwapChainBuilder>> mLogger; ///< Logger reference
        skr::Arc<skr::ServiceProvider>
            mServiceProvider; ///< Service provider reference
    };

} // namespace FREYA_NAMESPACE
