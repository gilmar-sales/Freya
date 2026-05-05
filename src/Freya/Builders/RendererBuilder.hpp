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
     * Supports both Forward and Deferred rendering strategies.
     * For deferred mode, constructs a DeferredCompressedPass internally
     * using the same SwapChain that the Renderer will use.
     */
    class RendererBuilder
    {
      public:
        RendererBuilder(const Ref<Instance>&             instance,
                        const Ref<Surface>&              surface,
                        const Ref<PhysicalDevice>&       physicalDevice,
                        const Ref<Device>&               device,
                        const Ref<CommandPool>&          commandPool,
                        const Ref<SwapChain>&            swapChain,
                        const Ref<RenderPass>&           renderPass,
                        const Ref<EventManager>&         eventManager,
                        const Ref<Window>&               window,
                        const Ref<FreyaOptions>&         freyaOptions,
                        const Ref<skr::ServiceProvider>& serviceProvider);

        /**
         * @brief Builds and returns the Renderer object.
         * For deferred mode, creates the DeferredCompressedPass with
         * the current SwapChain.
         * @return Shared pointer to created Renderer
         */
        Ref<Renderer> Build();

      private:
        friend class ApplicationBuilder;

        Ref<Instance>       mInstance;
        Ref<Surface>        mSurface;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Device>         mDevice;
        Ref<CommandPool>    mCommandPool;
        Ref<SwapChain>      mSwapChain;
        Ref<EventManager>   mEventManager;
        Ref<RenderPass>     mRenderPass;
        Ref<Window>         mWindow;
        Ref<FreyaOptions>   mFreyaOptions;

        Ref<skr::Logger<RendererBuilder>> mLogger;
        Ref<skr::ServiceProvider>         mServiceProvider;
    };

} // namespace FREYA_NAMESPACE
