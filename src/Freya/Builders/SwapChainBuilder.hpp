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
        SwapChainBuilder(const Ref<Instance>&       instance,
                         const Ref<PhysicalDevice>& physicalDevice,
                         const Ref<Device>&         device,
                         const Ref<Surface>&        surface,
                         const Ref<RenderPass>&     renderPass,
                         const Ref<FreyaOptions>&   freyaOptions,
                         const Ref<skr::Logger<SwapChainBuilder>>& logger,
                         const Ref<skr::ServiceProvider>& serviceProvider) :
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
        Ref<SwapChain> Build();

      protected:
        /**
         * @brief Chooses the best present mode based on VSync setting.
         * @return Selected present mode
         */
        vk::PresentModeKHR choosePresentMode();

      private:
        Ref<Instance>       mInstance;       ///< Instance reference
        Ref<PhysicalDevice> mPhysicalDevice; ///< Physical device reference
        Ref<Device>         mDevice;         ///< Device reference
        Ref<Surface>        mSurface;        ///< Surface reference
        Ref<RenderPass>     mRenderPass;     ///< Render pass reference
        Ref<FreyaOptions>   mFreyaOptions;   ///< Freya options reference

        Ref<skr::Logger<SwapChainBuilder>> mLogger; ///< Logger reference
        Ref<skr::ServiceProvider>
            mServiceProvider; ///< Service provider reference
    };

} // namespace FREYA_NAMESPACE
