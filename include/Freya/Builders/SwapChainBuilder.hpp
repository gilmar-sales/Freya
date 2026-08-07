#pragma once

#include "Freya/Core/SwapChain.hpp"

#include "Freya/FreyaOptions.hpp"

#include <optional>

namespace FREYA_NAMESPACE
{
    class Instance;
    class PhysicalDevice;
    class Device;
    class Surface;

    /**
     * @brief Builder for creating SwapChain objects.
     *
     * Queries surface capabilities and creates the swapchain with
     * image views plus presentation synchronization. Scene render
     * targets live on DeferredCompressedPass / CompositePass.
     */
    class SwapChainBuilder
    {
      public:
        SwapChainBuilder(
            const skr::Arc<Instance>&                      instance,
            const skr::Arc<PhysicalDevice>&                physicalDevice,
            const skr::Arc<Device>&                        device,
            const skr::Arc<Surface>&                       surface,
            const skr::Arc<FreyaOptions>&                  freyaOptions,
            const skr::Arc<skr::Logger<SwapChainBuilder>>& logger,
            const skr::Arc<skr::ServiceProvider>&          serviceProvider) :
            mInstance(instance), mPhysicalDevice(physicalDevice),
            mDevice(device), mSurface(surface), mFreyaOptions(freyaOptions),
            mLogger(logger), mServiceProvider(serviceProvider)
        {
        }

        /**
         * @brief Prefer this present mode when the surface supports it.
         * Overrides the default vSync priority list for the first choice.
         */
        SwapChainBuilder& PreferPresentMode(vk::PresentModeKHR mode)
        {
            mPreferredPresentMode = mode;
            return *this;
        }

        skr::Arc<SwapChain> Build();

      protected:
        vk::PresentModeKHR choosePresentMode();

      private:
        skr::Arc<Instance>       mInstance;
        skr::Arc<PhysicalDevice> mPhysicalDevice;
        skr::Arc<Device>         mDevice;
        skr::Arc<Surface>        mSurface;
        skr::Arc<FreyaOptions>   mFreyaOptions;

        skr::Arc<skr::Logger<SwapChainBuilder>> mLogger;
        skr::Arc<skr::ServiceProvider>          mServiceProvider;
        std::optional<vk::PresentModeKHR>       mPreferredPresentMode;
    };

} // namespace FREYA_NAMESPACE
