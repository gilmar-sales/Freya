#pragma once

#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/Window.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class InstanceBuilder;
    class DeferredCompressedPassBuilder;

    /**
     * @brief Builder for creating Renderer objects.
     *
     * Constructs the deferred scene and post-processing passes internally
     * using the same SwapChain that the Renderer will use.
     */
    class RendererBuilder
    {
      public:
        RendererBuilder(const skr::Arc<Instance>&             instance,
                        const skr::Arc<Surface>&              surface,
                        const skr::Arc<PhysicalDevice>&       physicalDevice,
                        const skr::Arc<Device>&               device,
                        const skr::Arc<CommandPool>&          commandPool,
                        const skr::Arc<SwapChain>&            swapChain,
                        const skr::Arc<EventManager>&         eventManager,
                        const skr::Arc<Window>&               window,
                        const skr::Arc<FreyaOptions>&         freyaOptions,
                        const skr::Arc<skr::ServiceProvider>& serviceProvider);

        /**
         * @brief Builds and returns the Renderer object.
         * Creates the DeferredCompressedPass with the current SwapChain.
         * @return Shared pointer to created Renderer
         */
        skr::Arc<Renderer> Build();

      private:
        friend class ApplicationBuilder;

        skr::Arc<Instance>       mInstance;
        skr::Arc<Surface>        mSurface;
        skr::Arc<PhysicalDevice> mPhysicalDevice;
        skr::Arc<Device>         mDevice;
        skr::Arc<CommandPool>    mCommandPool;
        skr::Arc<SwapChain>      mSwapChain;
        skr::Arc<EventManager>   mEventManager;
        skr::Arc<Window>         mWindow;
        skr::Arc<FreyaOptions>   mFreyaOptions;

        skr::Arc<skr::Logger<RendererBuilder>> mLogger;
        skr::Arc<skr::ServiceProvider>         mServiceProvider;
    };

} // namespace FREYA_NAMESPACE
