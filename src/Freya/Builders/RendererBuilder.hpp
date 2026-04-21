#pragma once

#include "Freya/FreyaOptions.hpp"

#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/Window.hpp"

namespace FREYA_NAMESPACE
{
    class InstanceBuilder;

    /**
     * @brief Builder for creating Renderer objects.
     *
     * @param instance       Instance reference
     * @param surface        Surface reference
     * @param physicalDevice  Physical device reference
     * @param device          Device reference
     * @param commandPool     Command pool reference
     * @param swapChain       Swapchain reference
     * @param renderPass      Render pass reference
     * @param eventManager    Event manager reference
     * @param window          Window reference
     * @param freyaOptions     Freya options reference
     * @param serviceProvider  Service provider reference
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
                        const Ref<skr::ServiceProvider>& serviceProvider) :
            mSurface(surface), mPhysicalDevice(physicalDevice), mDevice(device),
            mCommandPool(commandPool), mSwapChain(swapChain),
            mRenderPass(renderPass), mWindow(window),
            mEventManager(eventManager), mInstance(instance),
            mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider),
            mLogger(serviceProvider->GetService<skr::Logger<RendererBuilder>>())
        {
        }

        /**
         * @brief Builds and returns the Renderer object.
         * @return Shared pointer to created Renderer
         */
        Ref<Renderer> Build();

      private:
        friend class ApplicationBuilder;

        Ref<Instance>       mInstance;       ///< Instance reference
        Ref<Surface>        mSurface;        ///< Surface reference
        Ref<PhysicalDevice> mPhysicalDevice; ///< Physical device reference
        Ref<Device>         mDevice;         ///< Device reference
        Ref<CommandPool>    mCommandPool;    ///< Command pool reference
        Ref<SwapChain>      mSwapChain;      ///< Swapchain reference
        Ref<EventManager>   mEventManager;   ///< Event manager reference
        Ref<RenderPass>     mRenderPass;     ///< Render pass reference
        Ref<Window>         mWindow;         ///< Window reference
        Ref<FreyaOptions>   mFreyaOptions;   ///< Freya options reference

        Ref<skr::Logger<RendererBuilder>> mLogger; ///< Logger reference
        Ref<skr::ServiceProvider>
            mServiceProvider; ///< Service provider reference
    };

} // namespace FREYA_NAMESPACE
